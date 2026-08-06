import argparse
import socket
import struct
import time
from concurrent.futures import ThreadPoolExecutor, as_completed


HEADER_SIZE = 4
UNKNOWN_PACKET_TYPE = 0xFFFF


def make_packet(packet_type=UNKNOWN_PACKET_TYPE, payload=b"ping"):
    length = HEADER_SIZE + len(payload)
    return struct.pack("<HH", length, packet_type) + payload


def send_packets(host, port, timeout, packets, payload, delay):
    result = {
        "ok": True,
        "sent": 0,
        "connect_ms": 0.0,
        "send_ms": 0.0,
        "error": None,
    }

    sock = None
    try:
        started_at = time.perf_counter()

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))

        connected_at = time.perf_counter()

        packet = make_packet(payload=payload)

        for _ in range(packets):
            sock.sendall(packet)
            result["sent"] += 1

            if delay > 0:
                time.sleep(delay)

        finished_at = time.perf_counter()

        result["connect_ms"] = (connected_at - started_at) * 1000
        result["send_ms"] = (finished_at - connected_at) * 1000

    except Exception as exc:
        result["ok"] = False
        result["error"] = str(exc)
    finally:
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    return result


def main():
    parser = argparse.ArgumentParser(
        description="Run repeated packet send tests against ChannelServer."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--clients", type=int, default=10)
    parser.add_argument("--packets", type=int, default=100)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--payload", default="ping")
    parser.add_argument("--delay", type=float, default=0.0)
    args = parser.parse_args()

    payload = args.payload.encode("utf-8")
    total_expected = args.clients * args.packets

    print(f"target={args.host}:{args.port}")
    print(
        "clients="
        f"{args.clients}, packets_per_client={args.packets}, "
        f"total_expected={total_expected}, timeout={args.timeout}, delay={args.delay}"
    )

    started_at = time.perf_counter()
    results = []

    with ThreadPoolExecutor(max_workers=args.clients) as executor:
        futures = [
            executor.submit(
                send_packets,
                args.host,
                args.port,
                args.timeout,
                args.packets,
                payload,
                args.delay,
            )
            for _ in range(args.clients)
        ]

        for future in as_completed(futures):
            results.append(future.result())

    elapsed = time.perf_counter() - started_at

    success = [result for result in results if result["ok"]]
    failed = [result for result in results if not result["ok"]]
    total_sent = sum(result["sent"] for result in results)
    packets_per_sec = total_sent / elapsed if elapsed > 0 else 0

    print(f"success_clients={len(success)}")
    print(f"failed_clients={len(failed)}")
    print(f"total_sent={total_sent}")
    print(f"elapsed_sec={elapsed:.3f}")
    print(f"packets_per_sec={packets_per_sec:.3f}")

    if success:
        connect_times = [result["connect_ms"] for result in success]
        send_times = [result["send_ms"] for result in success]

        print(f"connect_ms_avg={sum(connect_times) / len(connect_times):.3f}")
        print(f"connect_ms_max={max(connect_times):.3f}")
        print(f"send_ms_avg={sum(send_times) / len(send_times):.3f}")
        print(f"send_ms_max={max(send_times):.3f}")

    if failed:
        print("errors:")
        for result in failed[:10]:
            print(f"- sent={result['sent']}, error={result['error']}")

    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()