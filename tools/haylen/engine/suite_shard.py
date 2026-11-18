import re
from dataclasses import dataclass

from haylen.core.errors.invalid_option import InvalidOption


@dataclass(frozen=True)
class SuiteShard:
    """One of the equal parts of a suite that machines run side by side, such as the second of four, which takes every item of the suite whose place, counted from the shard, falls on a multiple of the count, so the shards together run each item once."""

    index: int
    count: int

    @staticmethod
    def parse(text: str) -> SuiteShard:
        match = re.fullmatch(r"([0-9]+)/([0-9]+)", text)
        if match is None:
            raise InvalidOption(f'The shard "{text}" is not a part of the tests. Give it as "<index>/<count>", such as "2/4".')
        index = int(match.group(1))
        count = int(match.group(2))
        if not 1 <= index <= count:
            raise InvalidOption(f'The shard "{text}" names no part of the tests. Its index goes from 1 to its count.')
        return SuiteShard(index, count)

    def takes(self, place: int) -> bool:
        """Whether the shard runs the item at a place of the suite, counted from 0."""
        return place % self.count == self.index - 1

    def ctest_arguments(self) -> list[str]:
        """The arguments with which CTest runs the shard. CTest numbers the tests that the build discovered in one fixed order, from 1, and runs every test in a process of its own."""
        return ["--tests-information", f"{self.index},,{self.count}"]

    def describe(self) -> str:
        return f"{self.index}/{self.count}"
