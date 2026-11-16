from dataclasses import dataclass
from pathlib import Path

from haylen.core.configuration import Configuration


@dataclass(frozen=True)
class SdkRequest:
    """What `haylen sdk build` asks an engine port to build: the targets by name, the configuration, the architectures of each target, or the default ones when none are named, the revision of the engine sources and the folder to install the SDK into."""

    targets: tuple[str, ...]
    configuration: Configuration
    architectures: tuple[str, ...] | None
    revision: str
    folder: Path
