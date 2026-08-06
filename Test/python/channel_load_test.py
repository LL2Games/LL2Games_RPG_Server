import argparse
import socket
import time
from concurrent.futures import ThreadPoolExecutor, as_completed


def connect_once(host, port, timeout, hold_time):
    started_at = time.perf_counter()

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect((host, port))

        connected_at = time.perf_counter()
        time.sleep(hold_time)
        sock.close()

        return {
            "ok": True,
            "connect_ms": (connected_at - started_at) * 1000,
            "error": None,
        }
    except Exception as exc:
        return {
            "ok": False,
            "connect_ms": None,
            "error": str(exc),
        }


def main():
    parser = argparse.ArgumentParser(
        description="Run concurrent connection tests against ChannelServer."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--clients", type=int, default=10)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--hold-time", type=float, default=0.2)
    args = parser.parse_args()

    print(f"target={args.host}:{args.port}")
    print(f"clients={args.clients}, timeout={args.timeout}, hold_time={args.hold_time}")

    started_at = time.perf_counter()
    results = []

    with ThreadPoolExecutor(max_workers=args.clients) as executor:
        futures = [
            executor.submit(
                connect_once,
                args.host,
                args.port,
                args.timeout,
                args.hold_time,
            )
            for _ in range(args.clients)
        ]

        for future in as_completed(futures):
            results.append(future.result())

    elapsed = time.perf_counter() - started_at

    success = [result for result in results if result["ok"]]
    failed = [result for result in results if not result["ok"]]

    print(f"success={len(success)}")
    print(f"failed={len(failed)}")
    print(f"elapsed_sec={elapsed:.3f}")

    if success:
        connect_times = [result["connect_ms"] for result in success]
        avg_ms = sum(connect_times) / len(connect_times)
        max_ms = max(connect_times)
        min_ms = min(connect_times)

        print(f"connect_ms_min={min_ms:.3f}")
        print(f"connect_ms_avg={avg_ms:.3f}")
        print(f"connect_ms_max={max_ms:.3f}")

    if failed:
        print("errors:")
        for result in failed[:10]:
            print(f"- {result['error']}")

    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()