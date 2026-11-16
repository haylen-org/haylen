import hashlib
import json
import os
import shutil
import subprocess
import sys
from collections.abc import Mapping
from pathlib import Path

from haylen.core.host import Host
from haylen.core.repository import Repository


class ExtensionEnvironment:
    """A Python environment of its own that uv makes in a folder outside the repository, with the tooling of the checkout installed in editable mode and extensions installed from their folders, the way a developer installs a private backend or engine port, and the command line `haylen` of that environment, which finds the extensions through their entry points alone."""

    # The variable that names the jobs of the builds that the commands run, which a machine that shares its processors keeps low.
    JOBS = "HAYLEN_JOBS"

    def __init__(self, folder: Path) -> None:
        self.folder = folder

    @property
    def command(self) -> Path:
        scripts = self.folder / ("Scripts" if Host.system() == "windows" else "bin")
        return scripts / Host.executable("haylen")

    def create(self, editable: tuple[Path, ...], packages: tuple[Path, ...]) -> None:
        """Makes the environment with the Python that runs the tests and installs the tooling of the checkout with the extensions in one resolution, so they share the installed tooling."""
        uv = shutil.which("uv")
        subprocess.run([uv, "venv", self.folder, "--python", sys.executable, "--quiet"], check=True, timeout=300)
        installed = [part for path in (Repository.ROOT, *editable) for part in ("--editable", path)]
        subprocess.run([uv, "pip", "install", "--python", self.folder, "--quiet", *installed, *packages], check=True, timeout=600)

    def haylen(self, *arguments: object, environment: Mapping[str, str] | None = None, timeout: float = 600, build: bool = False) -> subprocess.CompletedProcess:
        """Runs a command of the environment from the root of the checkout, with the jobs of `HAYLEN_JOBS` for a command that builds, and with the variables of `environment` replacing those of the tests."""
        variables = {name: value for name, value in os.environ.items() if name != "VIRTUAL_ENV"}
        variables |= environment or {}
        jobs = ["--jobs", os.environ[ExtensionEnvironment.JOBS]] if build and ExtensionEnvironment.JOBS in os.environ else []
        command = [str(self.command), *(str(argument) for argument in arguments), *jobs]
        return subprocess.run(command, cwd=Repository.ROOT, env=variables, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout, check=False)

    @staticmethod
    def shown(completed: subprocess.CompletedProcess) -> str:
        return f"The command {' '.join(completed.args)} ended with {completed.returncode}.\n{completed.stdout}\n{completed.stderr}"

    @staticmethod
    def result(completed: subprocess.CompletedProcess) -> dict:
        """Returns the result of a command that ran with `--json` and succeeded."""
        assert completed.returncode == 0, ExtensionEnvironment.shown(completed)
        return json.loads(completed.stdout)["result"]

    @staticmethod
    def error(completed: subprocess.CompletedProcess) -> dict:
        """Returns the typed error of a command that ran with `--json` and failed."""
        assert completed.returncode == 1, ExtensionEnvironment.shown(completed)
        document = json.loads(completed.stdout)
        assert document["ok"] is False, ExtensionEnvironment.shown(completed)
        return document["error"]

    @staticmethod
    def digest(folder: Path) -> str:
        """Hashes the paths and contents of the files of a folder of the tooling, so a test sees that a command changed none of them."""
        hashed = hashlib.sha256()
        for path in sorted(path for path in folder.rglob("*") if path.is_file() and "__pycache__" not in path.parts):
            hashed.update(path.relative_to(folder).as_posix().encode() + b"\0" + path.read_bytes())
        return hashed.hexdigest()
