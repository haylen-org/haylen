import re
from dataclasses import dataclass


@dataclass(frozen=True, order=True)
class Version:
    """A version of one to three numbers separated by dots, such as `1.2.0` or the `16.3` of a system, compared number by number with the missing numbers as zero."""

    numbers: tuple[int, int, int]

    PATTERN = re.compile(r"\d+(\.\d+){0,2}")

    @staticmethod
    def is_valid(text: str) -> bool:
        return Version.PATTERN.fullmatch(text) is not None

    @staticmethod
    def parse(text: str) -> Version:
        if not Version.is_valid(text):
            raise ValueError(f'The text "{text}" is not a version. A version is one to three numbers separated by dots, such as "1.2.0".')
        parts = [int(part) for part in text.split(".")]
        padded = (*parts, 0, 0)
        return Version((padded[0], padded[1], padded[2]))

    def accepts(self, other: Version) -> bool:
        """Tells whether a version satisfies this version as a requirement: it is the same major version and not older."""
        return other.numbers[0] == self.numbers[0] and other >= self

    @property
    def text(self) -> str:
        return ".".join(str(number) for number in self.numbers)
