from pathlib import Path


class FileRange:
    """Reads one range of a file, so a server copies only the bytes that a response promised."""

    def __init__(self, path: Path, offset: int, length: int) -> None:
        self.stream = path.open("rb")
        self.stream.seek(offset)
        self.remaining = length

    def read(self, size: int = -1) -> bytes:
        wanted = self.remaining if size < 0 else min(size, self.remaining)
        data = self.stream.read(wanted)
        self.remaining -= len(data)
        return data

    def close(self) -> None:
        self.stream.close()
