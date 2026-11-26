import time


class Clock:
    """The time that the checks of the tooling bound their waits with: a monotonic reading in seconds and a sleep, which the tests replace with a clock that they move themselves."""

    def monotonic(self) -> float:
        return time.monotonic()

    def sleep(self, seconds: float) -> None:
        time.sleep(seconds)
