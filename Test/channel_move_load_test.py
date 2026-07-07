import argparse
import socket
import struct
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed

PKT_CHANNEL_AUTH = 0x0009
PKT_ENTER_MAP = 0x000A
PKT_PLAYER_MOVE = 0x0020
HEADER_SIZE = 4


def make_body(*fields):
    body = b""
    for field in fields:
        data = str(field).encode("utf-8")
        body += struct.pack("<H", len(data))
        body += data
    return body


def make_packet(packet_type, body):
    total_length = HEADER_SIZE + len(body)
    return struct.pack("<HH", total_length, packet_type) + body


def parse_fields(body):
    fields = []
    offset = 0

    while offset + 2 <= len(body):
        field_len = struct.unpack_from("<H", body, offset)[0]
        offset += 2

        if offset + field_len > len(body):
            break

        fields.append(body[offset:offset + field_len].decode("utf-8", errors="replace"))
        offset += field_len

    return fields


def parse_packets(buffer):
    packets = []
    offset = 0

    while offset + HEADER_SIZE <= len(buffer):
        packet_length, packet_type = struct.unpack_from("<HH", buffer, offset)

        if packet_length < HEADER_SIZE:
            break

        if offset + packet_length > len(buffer):
            break

        body = buffer[offset + HEADER_SIZE:offset + packet_length]
        packets.append((packet_type, body))
        offset += packet_length

    return packets, buffer[offset:]


def percentile(values, p):
    if not values:
        return 0.0

    sorted_values = sorted(values)
    index = int((len(sorted_values) - 1) * p)
    return sorted_values[index]


def wait_for_result(sock, target_packet_type, timeout):
    received = b""
    packets_seen = 0
    bytes_seen = 0
    deadline = time.perf_counter() + timeout

    while time.perf_counter() < deadline:
        remaining = max(0.001, deadline - time.perf_counter())
        sock.settimeout(remaining)

        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break

        if chunk == b"":
            break

        received += chunk
        bytes_seen += len(chunk)
        packets, received = parse_packets(received)
        packets_seen += len(packets)

        for packet_type, body in packets:
            if packet_type != target_packet_type:
                continue

            fields = parse_fields(body)
            if fields and fields[0] in ("ok", "nok"):
                return {
                    "success": fields[0] == "ok",
                    "result": fields[0],
                    "fields": fields,
                    "packets_seen": packets_seen,
                    "bytes_seen": bytes_seen,
                }

    return {
        "success": False,
        "result": "timeout",
        "fields": [],
        "packets_seen": packets_seen,
        "bytes_seen": bytes_seen,
    }


def make_move_packet(x, y, speed, direction):
    return make_packet(
        PKT_PLAYER_MOVE,
        make_body(f"{x:.3f}", f"{y:.3f}", f"{speed:.3f}", str(direction)),
    )


def run_client(args, character_id):
    start = time.perf_counter()

    result = {
        "character_id": character_id,
        "success": False,
        "auth_success": False,
        "enter_success": False,
        "auth_ms": 0.0,
        "enter_ms": 0.0,
        "move_sent": 0,
        "move_recv": 0,
        "packets_recv": 0,
        "bytes_recv": 0,
        "elapsed_ms": 0.0,
        "error": "",
    }

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(args.timeout)
            sock.connect((args.host, args.port))

            auth_start = time.perf_counter()
            sock.sendall(make_packet(PKT_CHANNEL_AUTH, make_body(str(character_id))))
            auth_result = wait_for_result(sock, PKT_CHANNEL_AUTH, args.timeout)
            result["auth_ms"] = (time.perf_counter() - auth_start) * 1000
            result["packets_recv"] += auth_result["packets_seen"]
            result["bytes_recv"] += auth_result["bytes_seen"]

            if not auth_result["success"]:
                result["error"] = f"auth_{auth_result['result']}"
                return result

            result["auth_success"] = True

            enter_start = time.perf_counter()
            sock.sendall(make_packet(PKT_ENTER_MAP, make_body(str(character_id), str(args.map_id))))
            enter_result = wait_for_result(sock, PKT_ENTER_MAP, args.timeout)
            result["enter_ms"] = (time.perf_counter() - enter_start) * 1000
            result["packets_recv"] += enter_result["packets_seen"]
            result["bytes_recv"] += enter_result["bytes_seen"]

            if not enter_result["success"]:
                result["error"] = f"enter_{enter_result['result']}"
                return result

            result["enter_success"] = True

            recv_buffer = b""
            sock.settimeout(args.recv_timeout)

            x = args.start_x + (character_id - args.start_character_id) * args.spawn_gap
            y = args.start_y
            direction = 1
            interval = 1.0 / args.moves_per_sec if args.moves_per_sec > 0 else args.duration
            end_at = time.perf_counter() + args.duration
            next_send = time.perf_counter()

            while time.perf_counter() < end_at:
                now = time.perf_counter()

                if args.moves_per_sec > 0 and now >= next_send:
                    x += args.step_x
                    y += args.step_y
                    sock.sendall(make_move_packet(x, y, args.speed, direction))
                    result["move_sent"] += 1
                    direction = (direction + 1) % 8
                    next_send += interval

                try:
                    chunk = sock.recv(8192)
                    if chunk == b"":
                        break

                    result["bytes_recv"] += len(chunk)
                    recv_buffer += chunk
                    packets, recv_buffer = parse_packets(recv_buffer)
                    result["packets_recv"] += len(packets)

                    for packet_type, _ in packets:
                        if packet_type == PKT_PLAYER_MOVE:
                            result["move_recv"] += 1

                except socket.timeout:
                    pass

            result["success"] = True
            return result

    except Exception as e:
        result["error"] = repr(e)
        return result

    finally:
        result["elapsed_ms"] = (time.perf_counter() - start) * 1000


def print_latency(name, values):
    if not values:
        return

    print(f"{name}_min={min(values):.3f}")
    print(f"{name}_avg={sum(values) / len(values):.3f}")
    print(f"{name}_p50={percentile(values, 0.50):.3f}")
    print(f"{name}_p95={percentile(values, 0.95):.3f}")
    print(f"{name}_p99={percentile(values, 0.99):.3f}")
    print(f"{name}_max={max(values):.3f}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--start-character-id", type=int, default=900000)
    parser.add_argument("--clients", type=int, default=100)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--recv-timeout", type=float, default=0.001)
    parser.add_argument("--map-id", type=int, default=100000000)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--moves-per-sec", type=float, default=5.0)
    parser.add_argument("--start-x", type=float, default=0.0)
    parser.add_argument("--start-y", type=float, default=0.0)
    parser.add_argument("--spawn-gap", type=float, default=0.1)
    parser.add_argument("--step-x", type=float, default=0.1)
    parser.add_argument("--step-y", type=float, default=0.0)
    parser.add_argument("--speed", type=float, default=5.0)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    character_ids = [
        args.start_character_id + i
        for i in range(args.clients)
    ]

    print(f"target={args.host}:{args.port}")
    print(
        f"start_character_id={args.start_character_id}, "
        f"clients={args.clients}, "
        f"map_id={args.map_id}, "
        f"duration={args.duration}, "
        f"moves_per_sec={args.moves_per_sec}, "
        f"timeout={args.timeout}"
    )

    started = time.perf_counter()
    results = []

    with ThreadPoolExecutor(max_workers=args.clients) as executor:
        futures = [
            executor.submit(run_client, args, character_id)
            for character_id in character_ids
        ]

        for future in as_completed(futures):
            results.append(future.result())

    elapsed_sec = time.perf_counter() - started

    success_results = [result for result in results if result["success"]]
    failed_results = [result for result in results if not result["success"]]
    auth_success_results = [result for result in results if result["auth_success"]]
    enter_success_results = [result for result in results if result["enter_success"]]

    auth_values = [result["auth_ms"] for result in auth_success_results]
    enter_values = [result["enter_ms"] for result in enter_success_results]
    move_sent_total = sum(result["move_sent"] for result in results)
    move_recv_total = sum(result["move_recv"] for result in results)
    packets_recv_total = sum(result["packets_recv"] for result in results)
    bytes_recv_total = sum(result["bytes_recv"] for result in results)

    print(f"success={len(success_results)}")
    print(f"failed={len(failed_results)}")
    print(f"auth_success={len(auth_success_results)}")
    print(f"enter_success={len(enter_success_results)}")
    print(f"elapsed_sec={elapsed_sec:.3f}")

    if failed_results:
        failure_by_error = Counter(result["error"] for result in failed_results)
        print("failure_by_error=" + ", ".join(
            f"{error}:{count}"
            for error, count in sorted(failure_by_error.items())
        ))

    print_latency("auth_ms", auth_values)
    print_latency("enter_ms", enter_values)
    print(f"move_sent_total={move_sent_total}")
    print(f"move_sent_per_sec={move_sent_total / elapsed_sec:.3f}")
    print(f"move_broadcast_recv_total={move_recv_total}")
    print(f"move_broadcast_recv_per_sec={move_recv_total / elapsed_sec:.3f}")
    print(f"packets_recv_total={packets_recv_total}")
    print(f"bytes_recv_total={bytes_recv_total}")
    print(f"bytes_recv_per_sec={bytes_recv_total / elapsed_sec:.3f}")

    if args.verbose or failed_results:
        for result in sorted(results, key=lambda item: item["character_id"]):
            status = "PASS" if result["success"] else "FAIL"
            print(
                f"[{status}] character_id={result['character_id']} "
                f"auth={result['auth_success']} "
                f"enter={result['enter_success']} "
                f"auth_ms={result['auth_ms']:.3f} "
                f"enter_ms={result['enter_ms']:.3f} "
                f"move_sent={result['move_sent']} "
                f"move_recv={result['move_recv']} "
                f"packets_recv={result['packets_recv']} "
                f"bytes_recv={result['bytes_recv']} "
                f"error={result['error']}"
            )

    if failed_results:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
