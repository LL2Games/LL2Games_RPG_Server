import argparse
import socket
import struct
import time


HEADER_SIZE = 4
BUFFER_SIZE = 1024
UNKNOWN_PACKET_TYPE = 0xFFFF

CASES = {}


def case(name, expect_open):
    def decorator(fn):
        CASES[name] = (fn, expect_open)
        return fn

    return decorator


def make_packet(packet_type=UNKNOWN_PACKET_TYPE, payload=b""):
    length = HEADER_SIZE + len(payload)
    return struct.pack("<HH", length, packet_type) + payload


def make_header(length, packet_type=UNKNOWN_PACKET_TYPE):
    return struct.pack("<HH", length, packet_type)


def connect(host, port, timeout):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    sock.connect((host, port))
    return sock


def connection_is_open(sock, timeout=0.3):
    old_timeout = sock.gettimeout()
    sock.settimeout(timeout)
    try:
        try:
            data = sock.recv(1, socket.MSG_PEEK)
        except (AttributeError, OSError):
            data = sock.recv(1)

        return data != b""
    except socket.timeout:
        return True
    except (ConnectionResetError, BrokenPipeError, OSError):
        return False
    finally:
        sock.settimeout(old_timeout)


def run_case(name, fn, expect_open, host, port, timeout, delay):
    try:
        is_open = fn(host, port, timeout, delay)
        passed = is_open == expect_open
    except Exception as exc:
        is_open = False
        passed = False
        print(f"[{name}] exception: {exc}")

    status = "PASS" if passed else "FAIL"
    expected = "open" if expect_open else "closed"
    actual = "open" if is_open else "closed"
    print(f"[{status}] {name}: expected {expected}, actual {actual}")
    return passed


@case("partial_header", expect_open=True)
def partial_header_case(host, port, timeout, delay):
    sock = connect(host, port, timeout)
    try:
        packet = make_packet(payload=b"abc")
        sock.sendall(packet[:1])
        time.sleep(delay)
        if not connection_is_open(sock):
            return False

        sock.sendall(packet[1:])
        time.sleep(delay)
        return connection_is_open(sock)
    finally:
        sock.close()


@case("partial_body", expect_open=True)
def partial_body_case(host, port, timeout, delay):
    sock = connect(host, port, timeout)
    try:
        packet = make_packet(payload=b"abcdef")
        split_at = HEADER_SIZE + 2
        sock.sendall(packet[:split_at])
        time.sleep(delay)
        if not connection_is_open(sock):
            return False

        sock.sendall(packet[split_at:])
        time.sleep(delay)
        return connection_is_open(sock)
    finally:
        sock.close()


@case("invalid_short_length", expect_open=False)
def invalid_short_length_case(host, port, timeout, delay):
    sock = connect(host, port, timeout)
    try:
        sock.sendall(make_header(HEADER_SIZE - 1))
        time.sleep(delay)
        return connection_is_open(sock)
    finally:
        sock.close()


@case("invalid_large_length", expect_open=False)
def invalid_large_length_case(host, port, timeout, delay):
    sock = connect(host, port, timeout)
    try:
        sock.sendall(make_header(BUFFER_SIZE + 1))
        time.sleep(delay)
        return connection_is_open(sock)
    finally:
        sock.close()


@case("coalesced_packets", expect_open=True)
def coalesced_packet_case(host, port, timeout, delay):
    sock = connect(host, port, timeout)
    try:
        first = make_packet(payload=b"first")
        second = make_packet(payload=b"second")
        sock.sendall(first + second)
        time.sleep(delay)
        return connection_is_open(sock)
    finally:
        sock.close()


def main():
    parser = argparse.ArgumentParser(
        description="Run TCP packet boundary tests against the ChannelServer."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--delay", type=float, default=0.3)
    parser.add_argument(
        "--case",
        choices=["all", *CASES.keys()],
        default="all",
        help="Run one test case or all cases.",
    )
    args = parser.parse_args()

    print(f"target={args.host}:{args.port}")
    print("Run ChannelServer first, then execute this script.")

    if args.case == "all":
        selected_cases = CASES.items()
    else:
        selected_cases = [(args.case, CASES[args.case])]

    passed = 0
    total = 0
    for name, (fn, expect_open) in selected_cases:
        total += 1
        if run_case(
            name,
            fn,
            expect_open,
            args.host,
            args.port,
            args.timeout,
            args.delay,
        ):
            passed += 1

    print(f"{passed}/{total} passed")


if __name__ == "__main__":
    main()
