import argparse
import socket
import struct
import time
from collections import Counter, deque
from concurrent.futures import ThreadPoolExecutor, as_completed

PKT_CHANNEL_AUTH = 0x0009
PKT_ENTER_MAP = 0x000A
PKT_MOVEMENT_INPUT = 0x002C
PKT_MOVEMENT_SNAPSHOT = 0x002D
HEADER_SIZE = 4


def make_body(*fields):
    body = b""
    for field in fields:
        data = str(field).encode("utf-8")
        body += struct.pack("!H", len(data))
        body += data
    return body


def make_packet(packet_type, body):
    total_length = HEADER_SIZE + len(body)
    return struct.pack("!HH", total_length, packet_type) + body


def parse_fields(body):
    fields = []
    offset = 0

    while offset + 2 <= len(body):
        field_len = struct.unpack_from("!H", body, offset)[0]
        offset += 2

        if offset + field_len > len(body):
            raise ValueError("truncated field")

        fields.append(body[offset:offset + field_len].decode("utf-8", errors="replace"))
        offset += field_len

    if offset != len(body):
        raise ValueError("trailing field header")
    return fields


def parse_packets(buffer):
    packets = []
    offset = 0

    while offset + HEADER_SIZE <= len(buffer):
        packet_length, packet_type = struct.unpack_from("!HH", buffer, offset)

        if packet_length < HEADER_SIZE or packet_length > 16 * 1024:
            raise ValueError("invalid packet length")

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

class PacketReader:
    """한 recv에 여러 패킷이 와도 대기 중인 패킷과 부분 바이트를 보존한다."""
    def __init__(self, sock):
        self.sock = sock
        self.buffer = b""
        self.pending = deque()
        self.packets_seen = 0
        self.bytes_seen = 0

    def next(self, timeout):
        deadline = time.perf_counter() + timeout
        while not self.pending:
            remaining = deadline - time.perf_counter()
            if remaining <= 0:
                raise socket.timeout()
            self.sock.settimeout(remaining)
            chunk = self.sock.recv(8192)
            if not chunk:
                raise ConnectionError("server disconnected")
            self.bytes_seen += len(chunk)
            self.buffer += chunk
            packets, self.buffer = parse_packets(self.buffer)
            self.packets_seen += len(packets)
            self.pending.extend(packets)
        packet_type, body = self.pending.popleft()
        return packet_type, parse_fields(body)

    def wait_result(self, packet_type, timeout):
        deadline = time.perf_counter() + timeout
        while time.perf_counter() < deadline:
            received_type, fields = self.next(max(0.001, deadline-time.perf_counter()))
            if received_type == packet_type and fields and fields[0] in ("ok", "nok"):
                if fields[0] != "ok":
                    raise RuntimeError(f"packet {packet_type:#x}: {fields}")
                return fields
        raise TimeoutError(f"packet {packet_type:#x}")


def make_move_packet(map_id, epoch, sequence, horizontal, vertical=0, jump=False):
    return make_packet(PKT_MOVEMENT_INPUT,
        make_body(map_id, epoch, sequence, horizontal, vertical, int(jump)))


def run_client(args, character_id, ticket, client_index):
    start = time.perf_counter()
    reader = None
    result = {
        "character_id": character_id, "success": False,
        "auth_success": False, "enter_success": False,
        "auth_ms": 0.0, "enter_ms": 0.0, "move_sent": 0, "move_recv": 0,
        "packets_recv": 0, "bytes_recv": 0, "elapsed_ms": 0.0, "error": "",
    }
    try:
        with socket.create_connection((args.host,args.port),args.timeout) as sock:
            reader = PacketReader(sock)
            auth_start = time.perf_counter()
            sock.sendall(make_packet(PKT_CHANNEL_AUTH,make_body(ticket)))
            reader.wait_result(PKT_CHANNEL_AUTH,args.timeout)
            result["auth_ms"] = (time.perf_counter()-auth_start)*1000
            result["auth_success"] = True
            enter_start = time.perf_counter()
            sock.sendall(make_packet(PKT_ENTER_MAP,b""))
            fields = reader.wait_result(PKT_ENTER_MAP,args.timeout)
            map_id = int(fields[1])
            result["enter_ms"] = (time.perf_counter()-enter_start)*1000
            result["enter_success"] = True

            # 최초 내 스냅샷의 epoch를 알아야 입력할 수 있다.
            epoch = None
            deadline = time.perf_counter()+args.timeout
            while epoch is None:
                kind, fields = reader.next(max(0.001,deadline-time.perf_counter()))
                if kind == PKT_MOVEMENT_SNAPSHOT and len(fields)==16 and \
                        int(fields[0])==map_id and fields[1]=="0" and int(fields[2])==character_id:
                    epoch = int(fields[3])
                if time.perf_counter() >= deadline and epoch is None:
                    raise TimeoutError("initial movement snapshot")

            sequence = 0
            acknowledged = False
            interval = 1.0/args.moves_per_sec if args.moves_per_sec>0 else args.duration
            next_send = time.perf_counter()
            end_at = next_send+args.duration
            while time.perf_counter()<end_at:
                now = time.perf_counter()
                if args.moves_per_sec>0 and now>=next_send:
                    sequence += 1
                    # 좌우를 번갈아 이동. 좌표나 속도는 보내지 않는다.
                    direction = 1 if (int(now-start)+client_index)%2==0 else -1
                    sock.sendall(make_move_packet(map_id,epoch,sequence,direction))
                    result["move_sent"] += 1
                    next_send = now+interval
                try:
                    kind, fields = reader.next(args.recv_timeout)
                except socket.timeout:
                    continue
                if kind == PKT_MOVEMENT_INPUT and fields and fields[0]=="nok":
                    raise RuntimeError(f"movement rejected: {fields}")
                if kind != PKT_MOVEMENT_SNAPSHOT or len(fields)!=16:
                    continue
                result["move_recv"] += 1
                if int(fields[0])!=map_id or fields[1]!="0" or int(fields[2])!=character_id:
                    continue
                if int(fields[14])<=0:
                    raise RuntimeError("player died during movement test")
                new_epoch = int(fields[3])
                if new_epoch != epoch:
                    epoch = new_epoch
                    sequence = 0
                if int(fields[5])>0:
                    acknowledged = True
            if args.moves_per_sec>0 and not acknowledged:
                raise RuntimeError("no input acknowledged by own snapshot")
            result["success"] = True
    except Exception as exception:
        result["error"] = repr(exception)
    finally:
        result["elapsed_ms"] = (time.perf_counter()-start)*1000
        if reader is not None:
            result["packets_recv"] = reader.packets_seen
            result["bytes_recv"] = reader.bytes_seen
    return result


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
    parser.add_argument("--tickets-file", required=True)
    parser.add_argument(
        "--clients",
        type=int,
        default=0,
        help="0이면 입장권 파일의 모든 항목 사용"
    )
    parser.add_argument("--workers", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--recv-timeout", type=float, default=0.001)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--moves-per-sec", type=float, default=5.0)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    if args.duration <= 0 or args.moves_per_sec < 0 or args.timeout <= 0 or args.recv_timeout <= 0:
        parser.error("duration/timeout/recv-timeout must be positive; moves-per-sec must be nonnegative")

    try:
        ticket_entries = load_ticket_entries(
            args.tickets_file
        )
    except (OSError, ValueError) as exception:
        print(
            f"[FAIL] 입장권 파일 로딩 실패: "
            f"{exception}"
        )
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
        f"duration={args.duration}, "
        f"moves_per_sec={args.moves_per_sec}, "
        f"timeout={args.timeout}"
    )

    started = time.perf_counter()
    results = []

    with ThreadPoolExecutor(
        max_workers=worker_count
    ) as executor:
        futures = [
            executor.submit(
                run_client,
                args,
                character_id,
                ticket,
                client_index
            )
            for client_index, (character_id, ticket)
            in enumerate(ticket_entries)
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
    print(f"movement_snapshot_recv_total={move_recv_total}")
    print(f"movement_snapshot_recv_per_sec={move_recv_total / elapsed_sec:.3f}")
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
