import xml.etree.ElementTree
from pathlib import Path

from haylen.core.repository import Repository
from haylen.terminal import Terminal


class GpuTestResults:
    """The results that a GPU test app writes on a device, an emulator or a simulator, as the XML of GoogleTest, with the images of the scenes that differ in folders next to them."""

    # The variables of the tests and of GoogleTest that a run hands to the tests on a device, in a simulator or in the test app of Android.
    VARIABLE_PREFIXES = ("HAYLEN_", "GTEST_")

    @staticmethod
    def problem(results: Path) -> str | None:
        """Says what went wrong when the tests failed or the app quit before it wrote its results, and nothing when they passed."""
        if not results.is_file():
            return "The GPU test app quit before it wrote its results, so a test crashed it or the system stopped it. Its output above and the crash logs of the device tell why."
        suites = xml.etree.ElementTree.parse(results).getroot()
        failed = int(suites.get("failures", "0")) + int(suites.get("errors", "0"))
        if failed:
            return f"{failed} of the {suites.get('tests', '0')} GPU tests failed on the device."
        return None

    @staticmethod
    def show_images(output: Path, terminal: Terminal) -> None:
        """Names the folder of the images of the scenes that differ, when the run wrote any."""
        if output.is_dir() and any(entry.is_dir() for entry in output.iterdir()):
            terminal.info(f"The images of the scenes that differ are in `{Repository.shown(output)}`.")
