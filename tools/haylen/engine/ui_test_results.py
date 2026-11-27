import datetime
import json
import re
from collections.abc import Sequence
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.terminal import Terminal


class UiTestResults:
    """The results of one run of the UI tests of Apple platforms, in a folder of their own under `results` of the folder of the runner, named after the time the run started and the device it runs on: the result bundle of xcodebuild, the samples of the app at each failure and, on a simulator, its diagnostics. A run that passes removes its folder, and a run that fails keeps it with `run.json`, which names the target, the device, the classes that the run asked for, every class when it names none, the tests that failed and the error of the run, so no later run replaces the results of a failure."""

    FOLDER = "results"
    BUNDLE = "tests.xcresult"
    RUN_FILE = "run.json"
    # The line of xcodebuild that ends a test that failed.
    FAILED = re.compile(r"Test Case '-\[(?:\w+\.)?(?P<suite>\w+) (?P<test>\w+)\]' failed")
    # The line of xcodebuild that reports a failure as it happens, while the app is still in the state that failed the test.
    FAILURE = re.compile(r": error: -\[\S+ \S+\] : ")

    def __init__(self, runner: Path, target: str, device: str, classes: Sequence[str], started: datetime.datetime) -> None:
        self.target = target
        self.device = device
        self.classes = list(classes)
        self.folder = runner / UiTestResults.FOLDER / f"{started:%Y-%m-%d-%H-%M-%S}-{UiTestResults.slug(device)}"
        self.failed: list[str] = []

    @staticmethod
    def slug(name: str) -> str:
        """A name in the form of the files of the repository, lowercase words joined by dashes, such as "iphone-18-pro" for "iPhone 18 Pro"."""
        return "-".join(re.findall(r"[a-z0-9]+", name.lower()))

    @property
    def bundle(self) -> Path:
        return self.folder / UiTestResults.BUNDLE

    def record(self, line: str) -> None:
        """Keeps the name of a test that a line of xcodebuild ends as failed."""
        match = UiTestResults.FAILED.search(line)
        if match is not None:
            self.failed.append(f"{match['suite']}.{match['test']}")

    def keep(self, error: BuildFailed, terminal: Terminal) -> None:
        """Keeps the folder of a run that failed with what `run.json` describes."""
        self.folder.mkdir(parents=True, exist_ok=True)
        run = {"target": self.target, "device": self.device, "classes": self.classes, "failed": self.failed, "error": error.message}
        (self.folder / UiTestResults.RUN_FILE).write_text(json.dumps(run, indent=2) + "\n", encoding="utf-8")
        terminal.info(f"The results of the failed UI tests are in `{Repository.shown(self.folder)}`.")

    def discard(self) -> None:
        """Removes the folder of a run that passed, whose results nobody needs."""
        Folders.remove(self.folder)
