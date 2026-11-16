from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class PackageRequest:
    """How a command asks a backend to package a built product for distribution: the format by name, such as `installer` or `steam`, the output folder and the options of the format."""

    format: str
    output: Path
    options: Mapping[str, object]
