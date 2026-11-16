import json
import threading
from collections.abc import Mapping
from typing import BinaryIO

from haylen.development.websocket import WebSocket


class DevelopmentClient:
    """One app connected to the development server, whose frames the watcher, the keys of the run and its own connection send in turn."""

    def __init__(self, stream: BinaryIO) -> None:
        self.stream = stream
        self.lock = threading.Lock()

    def send(self, opcode: int, payload: bytes) -> bool:
        """Sends one frame and tells whether the connection took it."""
        try:
            with self.lock:
                self.stream.write(WebSocket.frame(opcode, payload))
                self.stream.flush()
        except OSError:
            return False
        return True

    def send_batch(self, message: Mapping[str, object], contents: list[bytes]) -> bool:
        """Sends the JSON message of a batch and then the bytes of each of its files in one turn, so no other batch comes between them."""
        frames = [WebSocket.frame(WebSocket.TEXT, json.dumps(message).encode())]
        frames += [WebSocket.frame(WebSocket.BINARY, content) for content in contents]
        try:
            with self.lock:
                for frame in frames:
                    self.stream.write(frame)
                self.stream.flush()
        except OSError:
            return False
        return True
