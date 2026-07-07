import argparse
import socket
import struct
import time

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

        value = body[offset:offset + field_len].decode("utf-8", errors="replace")
        fields.append(value)
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


def run_auth_test(host, port, character_id, timeout):
    packet = make_packet(PKT_CHANNEL_AUTH, make_body(str(character_id)))

    received = b""
    start = time.perf_counter()

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
            except socket.timeout:
                break

    elapsed = time.perf_counter() - start
    packets = parse_packets(received)

    auth_result = None

    print(f"target={host}:{port}")
    print(f"character_id={character_id}")
    print(f"received_bytes={len(received)}")
    print(f"received_packets={len(packets)}")
    print(f"elapsed_sec={elapsed:.3f}")

    for index, (packet_type, body) in enumerate(packets, start=1):
        fields = parse_fields(body)

        if packet_type == PKT_CHANNEL_AUTH:
            print(f"packet[{index}] type=0x{packet_type:04X} fields={fields}")
        else:
            print(f"packet[{index}] type=0x{packet_type:04X} field_count={len(fields)} body_bytes={len(body)}")

        if packet_type == PKT_CHANNEL_AUTH and len(fields) > 0:
            auth_result = fields[0]

    if auth_result == "ok":
        print("[PASS] channel_auth: received ok")
        return 0

    if auth_result == "nok":
        print("[FAIL] channel_auth: received nok")
        return 1

    print("[FAIL] channel_auth: auth response not found")
    return 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--character-id", type=int, required=True)
    parser.add_argument("--timeout", type=float, default=2.0)
    args = parser.parse_args()

    raise SystemExit(run_auth_test(
        args.host,
        args.port,
        args.character_id,
        args.timeout
    ))


if __name__ == "__main__":
    main()