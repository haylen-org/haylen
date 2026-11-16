import json
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository


@dataclass(frozen=True)
class PortRecord:
    """The record `share/haylen/haylen-port.json` that the engine writes into the SDK of a port from its `haylen_port`: the platform, the contract of ports, the capabilities, the host tools, the app function, the release profile and the part of plugins."""

    FILE = Path("share") / "haylen" / "haylen-port.json"

    platform: str
    contract: int
    capabilities: frozenset[str]
    host_tools: tuple[str, ...]
    app_function: str
    release_profile: str
    plugin_part: str

    @staticmethod
    def load(installed: Path) -> PortRecord:
        path = installed / PortRecord.FILE
        if not path.is_file():
            raise BuildFailed(f'The SDK installed into `{Repository.shown(installed)}` holds no "{PortRecord.FILE.as_posix()}", so the engine was not built with the port. Build the SDK again.')
        document = json.loads(path.read_text(encoding="utf-8"))
        return PortRecord(
            platform=document["platform"],
            contract=int(document["contract"]),
            capabilities=frozenset(document["capabilities"]),
            host_tools=tuple(document["hostTools"]),
            app_function=document["appFunction"],
            release_profile=document["releaseProfile"],
            plugin_part=document["pluginPart"],
        )
