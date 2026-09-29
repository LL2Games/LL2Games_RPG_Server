"""실제 DB/서버 없이 새 이동 부하 도구의 wire protocol을 검증한다."""
import socket
import struct
import threading
import unittest
from types import SimpleNamespace

import channel_move_load_test as client


class MovementProtocolTest(unittest.TestCase):
    def test_input_network_order_and_fields(self):
        packet = client.make_move_packet(42, 3, 7, -1, 0, True)
        size, kind = struct.unpack("!HH", packet[:4])
        self.assertEqual(size, len(packet))
        self.assertEqual(kind, 0x002C)
        self.assertEqual(client.parse_fields(packet[4:]), ["42", "3", "7", "-1", "0", "1"])

    def test_fragmented_packets(self):
        first = client.make_move_packet(42, 3, 7, 1)
        second = client.make_move_packet(42, 3, 8, 0)
        rows, remainder = client.parse_packets(first[:3])
        self.assertEqual(rows, [])
        rows, remainder = client.parse_packets(remainder + first[3:] + second[:5])
        self.assertEqual(len(rows), 1)
        rows, remainder = client.parse_packets(remainder + second[5:])
        self.assertEqual(client.parse_fields(rows[0][1])[2], "8")
        self.assertEqual(remainder, b"")

    def test_wait_result_preserves_following_snapshot(self):
        a, b = socket.socketpair()
        with a, b:
            response = client.make_packet(client.PKT_ENTER_MAP, client.make_body("ok", 42, 0, 90))
            snapshot = client.make_packet(client.PKT_MOVEMENT_SNAPSHOT,
                client.make_body(42, 0, 1, 3, 0, 0, 0, 90, 0, 0, 0, 1, 0, 0, 100, 100))
            b.sendall(response + snapshot)
            reader = client.PacketReader(a)
            self.assertEqual(reader.wait_result(client.PKT_ENTER_MAP, 1)[0], "ok")
            kind, fields = reader.next(1)
            self.assertEqual(kind, client.PKT_MOVEMENT_SNAPSHOT)
            self.assertEqual(fields[3], "3")

    def test_load_client_uses_server_epoch_and_snapshot_ack(self):
        listener = socket.socket()
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(2)
        errors = []
        seen = []
        def server():
            try:
                conn, _ = listener.accept()
                with conn:
                    reader = client.PacketReader(conn)
                    kind, _ = reader.next(1)
                    self.assertEqual(kind, client.PKT_CHANNEL_AUTH)
                    conn.sendall(client.make_packet(kind, client.make_body("ok")))
                    kind, _ = reader.next(1)
                    self.assertEqual(kind, client.PKT_ENTER_MAP)
                    snapshot_fields = [42, 0, 1, 3, 0, 0, 0, 90, 0, 0, 0, 1, 0, 0, 100, 100]
                    conn.sendall(client.make_packet(kind, client.make_body("ok", 42, 0, 90)) +
                        client.make_packet(client.PKT_MOVEMENT_SNAPSHOT, client.make_body(*snapshot_fields)))
                    while True:
                        try:
                            kind, fields = reader.next(1)
                        except ConnectionError:
                            break
                        self.assertEqual(kind, client.PKT_MOVEMENT_INPUT)
                        self.assertEqual(fields[:2], ["42", "3"])
                        self.assertEqual(int(fields[2]), len(seen) + 1)
                        self.assertIn(fields[3], ["-1", "1"])
                        seen.append(fields)
                        snapshot_fields[4] += 1
                        snapshot_fields[5] = int(fields[2])
                        conn.sendall(client.make_packet(client.PKT_MOVEMENT_SNAPSHOT,
                            client.make_body(*snapshot_fields)))
            except Exception as exc:
                errors.append(exc)
        thread = threading.Thread(target=server, daemon=True)
        thread.start()
        try:
            args = SimpleNamespace(host="127.0.0.1", port=listener.getsockname()[1],
                timeout=1, recv_timeout=0.002, duration=0.08, moves_per_sec=100)
            result = client.run_client(args, 1, "test-ticket", 0)
            thread.join(2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(errors, [])
            self.assertTrue(result["success"], result["error"])
            self.assertGreater(result["move_sent"], 0)
            self.assertGreater(result["move_recv"], 0)
        finally:
            listener.close()


if __name__ == "__main__":
    unittest.main()
