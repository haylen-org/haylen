from dataclasses import dataclass
from enum import StrEnum


@dataclass(frozen=True)
class VerificationCheck:
    """One check of a built product: what it checked, whether it passed, failed or only warns, and the sentence that says what it found and what to do."""

    class Outcome(StrEnum):
        PASSED = "passed"
        WARNING = "warning"
        FAILED = "failed"

    name: str
    outcome: Outcome
    detail: str
    source: str

    @staticmethod
    def passed(name: str, detail: str, source: str) -> VerificationCheck:
        return VerificationCheck(name, VerificationCheck.Outcome.PASSED, detail, source)

    @staticmethod
    def failed(name: str, detail: str, source: str) -> VerificationCheck:
        return VerificationCheck(name, VerificationCheck.Outcome.FAILED, detail, source)

    @staticmethod
    def warning(name: str, detail: str, source: str) -> VerificationCheck:
        return VerificationCheck(name, VerificationCheck.Outcome.WARNING, detail, source)

    def describe(self) -> dict[str, str]:
        return {"name": self.name, "outcome": self.outcome.value, "detail": self.detail, "source": self.source}
