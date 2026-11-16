import codecs
import os
from typing import TextIO


class KeyReader:
    """Reads the keys that the developer presses in an interactive terminal one at a time and without echo, and leaves the terminal as it found it when it closes. A key is one character that it types, or the name of a key that edits, such as "Left", "Backspace" or "Enter"."""

    # How long an Escape waits for the rest of the sequence of an arrow or another key that edits, which the terminal sends at once.
    SEQUENCE_SECONDS = 0.05
    SEQUENCES = {"A": "Up", "B": "Down", "C": "Right", "D": "Left", "H": "Home", "F": "End", "1~": "Home", "7~": "Home", "4~": "End", "8~": "End", "3~": "Delete"}
    WINDOWS_KEYS = {"H": "Up", "P": "Down", "M": "Right", "K": "Left", "G": "Home", "O": "End", "S": "Delete"}
    CONTROLS = {"\r": "Enter", "\n": "Enter", "\t": "Tab", "\x7f": "Backspace", "\x08": "Backspace", "\x01": "Home", "\x05": "End", "\x15": "ClearLine"}

    def __init__(self, stream: TextIO) -> None:
        self.stream = stream
        self.saved: list | None = None
        self.decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")

    def open(self) -> None:
        if os.name == "nt":
            return
        import termios
        import tty

        self.saved = termios.tcgetattr(self.stream.fileno())
        tty.setcbreak(self.stream.fileno())

    def close(self) -> None:
        if self.saved is None:
            return
        import termios

        termios.tcsetattr(self.stream.fileno(), termios.TCSADRAIN, self.saved)
        self.saved = None

    def key(self) -> str:
        """Waits for the next key that it knows, and returns an empty text when the input ends."""
        while True:
            character = self.character()
            if not character:
                return ""
            key = self.name(character)
            if key:
                return key

    def name(self, character: str) -> str:
        if character in KeyReader.CONTROLS:
            return KeyReader.CONTROLS[character]
        if os.name == "nt" and character in ("\x00", "\xe0"):
            return KeyReader.WINDOWS_KEYS.get(self.character(), "")
        if character == "\x1b":
            return self.sequence()
        return character if character.isprintable() else ""

    def sequence(self) -> str:
        """Reads what follows an Escape: nothing for the key Escape itself, or the rest of the sequence of a key such as an arrow."""
        if os.name == "nt" or not self.waiting():
            return "Escape"
        if self.character() not in ("[", "O"):
            return ""
        body = ""
        while True:
            character = self.character()
            if not character or "@" <= character <= "~":
                break
            body += character
        return KeyReader.SEQUENCES.get(body + character, "")

    def waiting(self) -> bool:
        import select

        return bool(select.select([self.stream.fileno()], [], [], KeyReader.SEQUENCE_SECONDS)[0])

    def character(self) -> str:
        """Reads one character, gathering the bytes of a character that UTF-8 writes in several, and returns an empty text when the input ends."""
        if os.name == "nt":
            import msvcrt

            return msvcrt.getwch()
        while True:
            data = os.read(self.stream.fileno(), 1)
            if not data:
                return ""
            character = self.decoder.decode(data)
            if character:
                return character
