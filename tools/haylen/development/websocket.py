import base64
import hashlib
from typing import BinaryIO


class WebSocket:
    """The parts of RFC 6455 that the development server speaks: the answer to the handshake, the frames it sends, which are never masked, and the frames of the apps, which always are."""

    GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    TEXT = 0x1
    BINARY = 0x2
    CLOSE = 0x8
    PING = 0x9
    PONG = 0xA

    @staticmethod
    def accept(key: str) -> str:
        """Answers the key of a handshake the way RFC 6455 asks."""
        return base64.b64encode(hashlib.sha1((key + WebSocket.GUID).encode()).digest()).decode()

    @staticmethod
    def frame(opcode: int, payload: bytes) -> bytes:
        """Encodes one whole frame from the server."""
        size = len(payload)
        if size < 126:
            header = bytes([0x80 | opcode, size])
        elif size < 1 << 16:
            header = bytes([0x80 | opcode, 126]) + size.to_bytes(2, "big")
        else:
            header = bytes([0x80 | opcode, 127]) + size.to_bytes(8, "big")
        return header + payload

    @staticmethod
    def read_frame(stream: BinaryIO) -> tuple[bool, int, bytes] | None:
        """Reads one frame from a client and returns whether it ends its message, its opcode and its payload, or nothing once the connection ended."""
        head = stream.read(2)
        if len(head) < 2:
            return None

        size = head[1] & 0x7F
        if size == 126:
            size = int.from_bytes(stream.read(2), "big")
        elif size == 127:
            size = int.from_bytes(stream.read(8), "big")
        mask = stream.read(4) if head[1] & 0x80 else b"\0\0\0\0"
        payload = stream.read(size)
        if len(payload) < size:
            return None

        unmasked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        return bool(head[0] & 0x80), head[0] & 0x0F, unmasked
