import io
from unittest import mock

import pytest

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.websocket import WebSocket
from tests.development.development_fixture import DevelopmentFixture


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

    def test_a_frame_longer_than_the_largest_message_fails_before_its_payload_is_read(self):
        size = WebSocket.MAX_MESSAGE + 1
        head = bytes([0x80 | WebSocket.TEXT, 0x80 | 127]) + size.to_bytes(8, "big")
        stream = io.BytesIO(head + bytes(16))

        with pytest.raises(DevelopmentFailed, match=f"An app sent a message of {size} bytes"):
            WebSocket.read_frame(stream)
        assert stream.tell() == len(head)

    def test_a_message_of_frames_longer_than_the_largest_message_closes_the_connection_of_the_app(self, tmp_path, monkeypatch):
        monkeypatch.setattr(WebSocket, "MAX_MESSAGE", 10)
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        part = bytes([WebSocket.TEXT, 6]) + b'{"a":1'

        with pytest.raises(DevelopmentFailed, match="An app sent a message of 12 bytes"):
            server.serve(mock.Mock(), io.BytesIO(part + part))
