from haylen.core.errors.build_failed import BuildFailed
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.verification_report import VerificationReport
from haylen.terminal import Terminal


class ReportPrinter:
    """Prints the verification of a product: its warnings with what to do, and its failures, which end the command."""

    @staticmethod
    def verification(report: VerificationReport, terminal: Terminal, subject: str) -> None:
        for check in report.warnings:
            terminal.warning(check.detail)
        if not report.passed:
            lines = "\n".join(f"  {check.detail}" for check in report.failures)
            raise BuildFailed(f"The verification of {subject} failed:\n{lines}")
        passed = sum(1 for check in report.checks if check.outcome is VerificationCheck.Outcome.PASSED)
        terminal.success(f"The verification of {subject} passed {passed} checks.")
