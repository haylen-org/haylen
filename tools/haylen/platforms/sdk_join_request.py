from dataclasses import dataclass
from pathlib import Path

from haylen.sdk.sdk_manifest import SdkManifest


@dataclass(frozen=True)
class SdkJoinRequest:
    """What `haylen sdk join` asks an engine port to do: join the SDKs of the same port, engine sources and configuration that hold different slices, such as SDKs that several machines built side by side, into one SDK in a folder of its own."""

    manifests: tuple[SdkManifest, ...]
    folder: Path
