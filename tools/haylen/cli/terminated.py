import signal


class Terminated(BaseException):
    """A signal that ends the command without Ctrl+C, such as the one of a bound on its time. It unwinds the command like Ctrl+C, so the processes, servers and apps that the command started stop with it."""

    SIGNALS = ("SIGTERM", "SIGHUP", "SIGALRM")

    def __init__(self, number: int) -> None:
        super().__init__(number)
        self.number = number

    @staticmethod
    def install() -> None:
        for name in Terminated.SIGNALS:
            if hasattr(signal, name):
                signal.signal(getattr(signal, name), Terminated.handle)

    @staticmethod
    def handle(number: int, _frame: object) -> None:
        raise Terminated(number)
