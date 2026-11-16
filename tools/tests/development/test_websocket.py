import io

from haylen.development.websocket import WebSocket


class WebSocketTest:
    def test_the_accept_key_follows_rfc_6455(self):
        assert WebSocket.accept("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="

    def test_frames_encode_and_masked_frames_decode_at_every_length(self):
        mask = bytes([1, 2, 3, 4])
        for size in (5, 300, 70_000):
            payload = bytes(index % 251 for index in range(size))
            assert WebSocket.read_frame(io.BytesIO(WebSocket.frame(WebSocket.BINARY, payload))) == (True, WebSocket.BINARY, payload)

            if size < 126:
                length = bytes([0x80 | size])
            elif size < 1 << 16:
                length = bytes([0x80 | 126]) + size.to_bytes(2, "big")
            else:
                length = bytes([0x80 | 127]) + size.to_bytes(8, "big")
            masked = bytes([0x80 | WebSocket.TEXT]) + length + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
            assert WebSocket.read_frame(io.BytesIO(masked)) == (True, WebSocket.TEXT, payload)

        assert WebSocket.read_frame(io.BytesIO(b"\x81")) is None
