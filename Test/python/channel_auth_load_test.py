import argparse
import socket
import struct
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed

PKT_CHANNEL_AUTH = 0x0009
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

    return packets

def percentile(values, p):
    if not values:
        return 0.0

    sorted_values = sorted(values)
    index = int((len(sorted_values) - 1) * p)
    return sorted_values[index]

def load_ticket_entries(file_path):
    entries = []
    character_ids = set()
    tickets = set()

    with open(file_path, "r", encoding="utf-8") as ticket_file:
        for line_number, line in enumerate(ticket_file, start=1):
            stripped_line = line.strip()

            if not stripped_line:
                continue

            parts = stripped_line.split()

            if len(parts) != 2:
                raise ValueError(
                    f"{line_number}번째 줄 형식 오류: "
                    "character_id ticket 형식이어야 함"
                )

            character_id_text, ticket = parts

            try:
                character_id = int(character_id_text)
            except ValueError as exception:
                raise ValueError(
                    f"{line_number}번째 줄의 캐릭터 ID가 올바르지 않음"
                ) from exception

            if character_id <= 0:
                raise ValueError(
                    f"{line_number}번째 줄의 캐릭터 ID가 양수가 아님"
                )

            is_valid_ticket = (
                len(ticket) == 64
                and all(
                    character in "0123456789abcdef"
                    for character in ticket
                )
            )

            if not is_valid_ticket:
                raise ValueError(
                    f"{line_number}번째 줄의 입장권 형식이 올바르지 않음"
                )

            if character_id in character_ids:
                raise ValueError(
                    f"중복 캐릭터 ID 발견: {character_id}"
                )

            if ticket in tickets:
                raise ValueError(
                    f"중복 입장권 발견: {ticket}"
                )

            character_ids.add(character_id)
            tickets.add(ticket)
            entries.append((character_id, ticket))

    if not entries:
        raise ValueError("입장권 파일이 비어 있음")

    return entries


def auth_once(host,port,character_id,ticket,timeout,hold_seconds):
    packet = make_packet(PKT_CHANNEL_AUTH,make_body(ticket))
    received = b""

    start = time.perf_counter()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(timeout)
            sock.connect((host, port))
            sock.sendall(packet)

            while True:
                try:
                    chunk = sock.recv(4096)
                    if chunk == b"":
                        break

                    received += chunk
                    packets = parse_packets(received)

                    for packet_type, body in packets:
                        if packet_type != PKT_CHANNEL_AUTH:
                            continue

                        fields = parse_fields(body)
                        if len(fields) == 0:
                            continue

                        if fields[0] in ("ok", "nok"):
                            elapsed_ms = (time.perf_counter() - start) * 1000

                            if hold_seconds > 0:
                                time.sleep(hold_seconds)

                            return {
                                "character_id": character_id,
                                "success": fields[0] == "ok",
                                "result": fields[0],
                                "name": fields[1] if len(fields) > 1 else None,
                                "elapsed_ms": elapsed_ms,
                                "received_bytes": len(received),
                                "received_packets": len(packets),
                                "error": "",
                            }

                except socket.timeout:
                    break

        elapsed_ms = (time.perf_counter() - start) * 1000
        packets = parse_packets(received)

        return {
            "character_id": character_id,
            "success": False,
            "result": "no_auth_response",
            "name": None,
            "elapsed_ms": elapsed_ms,
            "received_bytes": len(received),
            "received_packets": len(packets),
            "error": "",
        }

    except Exception as e:
        elapsed_ms = (time.perf_counter() - start) * 1000
        return {
            "character_id": character_id,
            "success": False,
            "result": "exception",
            "name": None,
            "elapsed_ms": elapsed_ms,
            "received_bytes": len(received),
            "received_packets": 0,
            "error": repr(e),
        }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--tickets-file", required=True)
    parser.add_argument(
        "--clients",
        type=int,
        default=0,
        help="0이면 입장권 파일의 모든 항목 사용"
    )
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--hold-seconds", type=float, default=0.0)
    parser.add_argument("--workers", type=int, default=0)
    parser.add_argument("--ramp-seconds", type=float, default=0.0)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    try:
        ticket_entries = load_ticket_entries(args.tickets_file)
    except (OSError, ValueError) as exception:
        print(f"[FAIL] 입장권 파일 로딩 실패: {exception}")
        raise SystemExit(1)

    if args.clients < 0:
        print("[FAIL] clients는 0 이상의 값이어야 함")
        raise SystemExit(1)

    if args.clients > len(ticket_entries):
        print(
            f"[FAIL] 요청 클라이언트 수({args.clients})가 "
            f"입장권 수({len(ticket_entries)})보다 많음"
        )
        raise SystemExit(1)

    if args.clients > 0:
        ticket_entries = ticket_entries[:args.clients]

    client_count = len(ticket_entries)

    worker_count = (
        args.workers
        if args.workers > 0
        else client_count
    )
    worker_count = max(
        1,
        min(worker_count, client_count)
    )

    print(f"target={args.host}:{args.port}")
    print(f"tickets_file={args.tickets_file}")
    print(
        f"clients={client_count}, "
        f"workers={worker_count}, "
        f"timeout={args.timeout}, "
        f"hold_seconds={args.hold_seconds}, "
        f"ramp_seconds={args.ramp_seconds}"
    )

    started = time.perf_counter()
    results = []

    with ThreadPoolExecutor(max_workers=worker_count) as executor:
        futures = []
        ramp_delay = 0.0

        if args.ramp_seconds > 0 and client_count > 1:
            ramp_delay = (
                args.ramp_seconds /
                (client_count - 1)
            )

        for character_id, ticket in ticket_entries:
            futures.append(
                executor.submit(
                    auth_once,
                    args.host,
                    args.port,
                    character_id,
                    ticket,
                    args.timeout,
                    args.hold_seconds,
                )
            )

            if ramp_delay > 0:
                time.sleep(ramp_delay)

        for future in as_completed(futures):
            results.append(future.result())

    elapsed_sec = time.perf_counter() - started

    success_results = [result for result in results if result["success"]]
    failed_results = [result for result in results if not result["success"]]

    elapsed_values = [result["elapsed_ms"] for result in results]
    packet_values = [result["received_packets"] for result in results]
    byte_values = [result["received_bytes"] for result in results]

    print(f"success={len(success_results)}")
    print(f"failed={len(failed_results)}")
    print(f"elapsed_sec={elapsed_sec:.3f}")

    if failed_results:
        failure_by_result = Counter(result["result"] for result in failed_results)
        print("failure_by_result=" + ", ".join(
            f"{result}:{count}"
            for result, count in sorted(failure_by_result.items())
        ))

        failure_by_error = Counter(
            result["error"]
            for result in failed_results
            if result["error"]
        )
        if failure_by_error:
            print("failure_by_error=" + ", ".join(
                f"{error}:{count}"
                for error, count in failure_by_error.most_common(5)
            ))

    if elapsed_values:
        print(f"auth_ms_min={min(elapsed_values):.3f}")
        print(f"auth_ms_avg={sum(elapsed_values) / len(elapsed_values):.3f}")
        print(f"auth_ms_p50={percentile(elapsed_values, 0.50):.3f}")
        print(f"auth_ms_p95={percentile(elapsed_values, 0.95):.3f}")
        print(f"auth_ms_p99={percentile(elapsed_values, 0.99):.3f}")
        print(f"auth_ms_max={max(elapsed_values):.3f}")

    if packet_values:
        print(f"received_packets_avg={sum(packet_values) / len(packet_values):.3f}")
        print(f"received_packets_max={max(packet_values)}")

    if byte_values:
        print(f"received_bytes_avg={sum(byte_values) / len(byte_values):.3f}")
        print(f"received_bytes_max={max(byte_values)}")

    if args.verbose or failed_results:
        for result in sorted(results, key=lambda item: item["character_id"]):
            status = "PASS" if result["success"] else "FAIL"
            print(
                f"[{status}] character_id={result['character_id']} "
                f"result={result['result']} "
                f"name={result['name']} "
                f"elapsed_ms={result['elapsed_ms']:.3f} "
                f"packets={result['received_packets']} "
                f"bytes={result['received_bytes']} "
                f"error={result['error']}"
            )

    if failed_results:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
