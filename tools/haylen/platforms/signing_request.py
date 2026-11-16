from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class SigningRequest:
    """How a command asks a backend to create the signing identity of an app: the app folder, the kind of key, such as `release` or `debug`, and the options of the key, such as its alias."""

    app_folder: Path
    kind: str
    options: Mapping[str, object]
