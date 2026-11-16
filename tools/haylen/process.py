import collections
import subprocess
from collections.abc import Mapping, Sequence
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.repository import Repository
from haylen.terminal import Terminal


class Process:
    """Runs the native tools of the builds. A tool whose output the developer reads, such as the results of tests, streams it, and so does every tool with "--verbose". Any other tool writes its output to the log of the command, shows its latest line in place and on a failure prints its last lines with where the whole output is."""

    FAILURE_LINES = 60

    def __init__(self, terminal: Terminal) -> None:
        self.terminal = terminal

    @staticmethod
    def missing(executable: str) -> MissingToolchain:
        return MissingToolchain(f'The command "{Path(executable).name}" was not found. Install it, or add its folder to "PATH".')

    def run(self, command: Sequence[object], *, cwd: Path = Repository.ROOT, env: Mapping[str, str] | None = None, stream: bool = False) -> None:
        """Runs a command and fails with an error that names it when it fails."""
        parts = [str(part) for part in command]
        name = Path(parts[0]).name
        if not (stream or self.terminal.verbose):
            self.run_quietly(parts, cwd, env, name)
            return

        if self.terminal.verbose:
            self.terminal.command(parts)
        # The input of the terminal belongs to the tooling, which reads the lines of the developer console from it, so a tool never takes those lines.
        try:
            subprocess.run(parts, cwd=cwd, env=env, stdin=subprocess.DEVNULL, stdout=self.terminal.tool_output, check=True)
        except FileNotFoundError as error:
            raise Process.missing(parts[0]) from error
        except subprocess.CalledProcessError as error:
            if error.returncode < 0:
                raise BuildFailed(f'The command "{name}" was stopped by the signal {-error.returncode}.') from error
            raise BuildFailed(f'The command "{name}" failed with exit code {error.returncode}, and its output above shows why.') from error

    def run_quietly(self, parts: list[str], cwd: Path, env: Mapping[str, str] | None, name: str) -> None:
        log_path = self.terminal.log
        log_path.parent.mkdir(parents=True, exist_ok=True)
        tail: collections.deque[str] = collections.deque(maxlen=Process.FAILURE_LINES)
        with log_path.open("a", encoding="utf-8") as log:
            log.write(f"$ {Terminal.command_line(parts)}\n")
            try:
                process = subprocess.Popen(
                    parts,
                    cwd=cwd,
                    env=env,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                )
            except FileNotFoundError as error:
                raise Process.missing(parts[0]) from error
            with process:
                try:
                    for line in process.stdout:
                        log.write(line)
                        tail.append(line.rstrip("\n"))
                        if line.strip():
                            self.terminal.progress(line)
                    code = process.wait()
                finally:
                    self.terminal.end_progress()

        if code < 0:
            raise BuildFailed(f'The command "{name}" was stopped by the signal {-code}.')
        if code != 0:
            raise BuildFailed(
                f'The command "{name}" failed with exit code {code}. Its last lines follow, and `{Repository.shown(log_path)}` holds its whole output.',
                details="\n".join(tail),
            )

    def capture(self, command: Sequence[object], *, cwd: Path = Repository.ROOT, env: Mapping[str, str] | None = None) -> str:
        """Runs a command quietly and returns its standard output, and fails with its error output when it fails."""
        parts = [str(part) for part in command]
        try:
            completed = subprocess.run(parts, cwd=cwd, env=env, check=True, capture_output=True, text=True, encoding="utf-8", errors="replace")
        except FileNotFoundError as error:
            raise Process.missing(parts[0]) from error
        except subprocess.CalledProcessError as error:
            raise BuildFailed(f'The command "{Path(parts[0]).name}" failed with exit code {error.returncode}.', details=(error.stderr or error.stdout).strip()) from error
        return completed.stdout

    def attempt(self, command: Sequence[object]) -> tuple[int, bytes]:
        """Runs a command whose failure is an answer, such as a tool that reports nothing for an unsigned file, and returns its exit status with its standard output as bytes. Only a missing tool fails."""
        parts = [str(part) for part in command]
        try:
            completed = subprocess.run(parts, capture_output=True, check=False)
        except FileNotFoundError as error:
            raise Process.missing(parts[0]) from error
        return completed.returncode, completed.stdout
