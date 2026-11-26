from haylen.clock import Clock


class ManualClock(Clock):
    """A clock that only a sleep or the test moves forward, so the bounds of a check pass without waiting and never depend on how fast the machine runs."""

    def __init__(self) -> None:
        self.now = 0.0

    def monotonic(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds
