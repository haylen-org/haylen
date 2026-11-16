from collections.abc import Mapping
from dataclasses import dataclass

from haylen.process import Process
from haylen.terminal import Terminal


@dataclass(frozen=True)
class ToolingContext:
    """What every command, backend and engine port of the tooling shares: the terminal that reports the steps, the runner of native tools, the jobs that a build may run at once and the environment of the command."""

    terminal: Terminal
    process: Process
    jobs: int
    environment: Mapping[str, str]
