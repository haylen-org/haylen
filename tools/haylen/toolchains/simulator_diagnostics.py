import json
import shutil
import time
from pathlib import Path

from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext


class SimulatorDiagnostics:
    """What a simulator and the service of the simulators wrote from the moment it was made, which a failed run keeps in a folder, so a run that nobody watched, such as one of continuous integration, tells why the simulator stopped or what the system did meanwhile. The simulator writes its unified log into its own log store, which the Mac reads also once the simulator is shut down, the service of the simulators writes its log on the Mac, and the Mac keeps a crash report of every process of the simulator that crashed."""

    SERVICE_LOG = Path.home() / "Library" / "Logs" / "CoreSimulator" / "CoreSimulator.log"
    CRASH_REPORTS = Path.home() / "Library" / "Logs" / "DiagnosticReports"
    # The files of a kept folder: the simulator with its state as the run ended, its unified log, the log of the service of the simulators and the crash reports of its processes.
    SIMULATOR_FILE = "simulator.json"
    LOG_FILE = "simulator.log"
    SERVICE_FILE = "core-simulator.log"
    CRASH_FOLDER = "crash-reports"

    def __init__(self, context: ToolingContext, simulator: dict) -> None:
        self.context = context
        self.simulator = simulator
        self.started = time.time()
        self.service_offset = SimulatorDiagnostics.SERVICE_LOG.stat().st_size if SimulatorDiagnostics.SERVICE_LOG.exists() else 0

    @staticmethod
    def log_command(data: Path, started: float) -> list[str]:
        """The command that writes the unified log of a simulator from a moment on, read from the log store in the data folder of the simulator, which needs no running simulator."""
        return ["log", "show", "-D", str(data / "var" / "db" / "diagnostics"), "--start", f"@{int(started)}", "--style", "compact"]

    @staticmethod
    def service_log(text: bytes, offset: int) -> bytes:
        """The part of the log of the service of the simulators that it wrote after it held that many bytes, or all of it when the service started a new log meanwhile."""
        return text[offset:] if len(text) >= offset else text

    @staticmethod
    def crash_reports(folder: Path, udid: str, started: float) -> list[Path]:
        """The crash reports that the Mac wrote since a moment for processes of the simulator, whose reports name the folder of the simulator."""
        if not folder.is_dir():
            return []
        recent = [path for path in sorted(folder.iterdir()) if path.is_file() and path.stat().st_mtime >= started]
        return [path for path in recent if udid in path.read_text(encoding="utf-8", errors="replace")]

    def keep(self, folder: Path, state: str | None) -> Path:
        """Keeps the diagnostics of the simulator in a folder with the state it had as the run ended, None for a simulator that is no longer listed, and returns the folder."""
        folder.mkdir(parents=True, exist_ok=True)
        udid = self.simulator["udid"]
        described = {"name": self.simulator["name"], "udid": udid, "stateAtTheEnd": state or "missing"}
        (folder / SimulatorDiagnostics.SIMULATOR_FILE).write_text(json.dumps(described, indent=2) + "\n", encoding="utf-8")

        code, log = self.context.process.attempt(SimulatorDiagnostics.log_command(Path(self.simulator["dataPath"]), self.started))
        (folder / SimulatorDiagnostics.LOG_FILE).write_bytes(log)
        if code != 0:
            self.context.terminal.warning(f'The unified log of the simulator "{self.simulator["name"]}" could not be read, since "log" failed with exit code {code}.')

        if SimulatorDiagnostics.SERVICE_LOG.exists():
            service = SimulatorDiagnostics.service_log(SimulatorDiagnostics.SERVICE_LOG.read_bytes(), self.service_offset)
            (folder / SimulatorDiagnostics.SERVICE_FILE).write_bytes(service)

        reports = SimulatorDiagnostics.crash_reports(SimulatorDiagnostics.CRASH_REPORTS, udid, self.started)
        if reports:
            (folder / SimulatorDiagnostics.CRASH_FOLDER).mkdir(exist_ok=True)
            for report in reports:
                shutil.copy2(report, folder / SimulatorDiagnostics.CRASH_FOLDER / report.name)
        self.context.terminal.info(f'The logs of the simulator "{self.simulator["name"]}" are in `{Repository.shown(folder)}`.')
        return folder
