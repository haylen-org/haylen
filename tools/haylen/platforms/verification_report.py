from dataclasses import dataclass

from haylen.platforms.verification_check import VerificationCheck


@dataclass(frozen=True)
class VerificationReport:
    """The one report of a built product, which joins the generic checks of the core with the checks that the backend adds for its platform, so the two never give contradicting verdicts."""

    checks: tuple[VerificationCheck, ...]

    @property
    def passed(self) -> bool:
        return all(check.outcome is not VerificationCheck.Outcome.FAILED for check in self.checks)

    @property
    def failures(self) -> tuple[VerificationCheck, ...]:
        return tuple(check for check in self.checks if check.outcome is VerificationCheck.Outcome.FAILED)

    @property
    def warnings(self) -> tuple[VerificationCheck, ...]:
        return tuple(check for check in self.checks if check.outcome is VerificationCheck.Outcome.WARNING)

    def joined(self, other: VerificationReport) -> VerificationReport:
        return VerificationReport((*self.checks, *other.checks))

    def describe(self) -> dict[str, object]:
        return {"passed": self.passed, "checks": [check.describe() for check in self.checks]}
