class HaylenError(Exception):
    """A failure that the tooling expects. It ends the command with its sentence, the file and key it comes from and the output of the tool that failed, instead of a traceback, and its code names its kind in structured output."""

    code = "failed"

    def __init__(self, message: str, *, origin: str | None = None, details: str = "") -> None:
        super().__init__(message)
        self.message = message
        self.origin = origin
        self.details = details

    def describe(self) -> dict[str, str]:
        """Returns the error as the fields of structured output."""
        described = {"code": self.code, "message": self.message}
        if self.origin:
            described["origin"] = self.origin
        if self.details:
            described["details"] = self.details
        return described
