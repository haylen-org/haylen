import json
from collections.abc import Callable
from typing import BinaryIO


class DebugMessage:
    """The messages of the Debug Adapter Protocol that the debugger relay carries: a header with the length of the body, then the body in JSON."""

    @staticmethod
    def read(stream: BinaryIO) -> dict | None:
        """Reads one message, and returns nothing once the connection ended. A header without a whole length or a body that is not a JSON object fails with a `ValueError` whose message says which."""
        length = None
        while True:
            line = stream.readline()
            if not line:
                return None
            line = line.strip()
            if not line:
                break
            name, _, value = line.decode("ascii", errors="replace").partition(":")
            if name.strip().lower() == "content-length":
                length = DebugMessage.read_length(value.strip())

        if length is None:
            return None
        body = stream.read(length)
        if len(body) < length:
            return None
        try:
            message = json.loads(body)
        except ValueError as error:
            raise ValueError(f"The editor sent a message whose body is not JSON: {error}.") from error
        if not isinstance(message, dict):
            raise ValueError("The editor sent a message whose body is not a JSON object.")
        return message

    @staticmethod
    def read_length(text: str) -> int:
        if not text.isdigit():
            raise ValueError(f'The editor sent a message whose header "Content-Length" is "{text}", not a whole number of bytes.')
        return int(text)

    @staticmethod
    def frame(message: dict) -> bytes:
        body = json.dumps(message).encode()
        return f"Content-Length: {len(body)}\r\n\r\n".encode() + body

    @staticmethod
    def source_paths(value: object, convert: Callable[[str], str]) -> object:
        """Returns the message with the path of every source in it turned by the function, such as the path of a breakpoint or of a frame of the stack."""
        if isinstance(value, list):
            return [DebugMessage.source_paths(item, convert) for item in value]
        if not isinstance(value, dict):
            return value

        converted = {key: DebugMessage.source_paths(item, convert) for key, item in value.items()}
        source = converted.get("source")
        if isinstance(source, dict) and isinstance(source.get("path"), str):
            converted["source"] = {**source, "path": convert(source["path"])}
        return converted
