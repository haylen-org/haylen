from dataclasses import dataclass
from pathlib import Path

from haylen.core.configuration import Configuration


@dataclass(frozen=True)
class ProductRequest:
    """What a command asks to build: the app folder, the target by name, the configuration of the app and of the engine SDK, the architectures, or the default ones of the target when it names none, and the protected release that a Release build follows, such as the release that shipped last, which continuous integration keeps, or none to follow the last release of the build folder."""

    app_folder: Path
    target: str
    configuration: Configuration
    engine_configuration: Configuration
    architectures: tuple[str, ...] | None
    previous_release: Path | None = None
