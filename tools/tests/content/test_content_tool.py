import json
from pathlib import Path

import pytest

from haylen.content.content_tool import ContentTool
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.host import Host
from haylen.sdk.sdk_manifest import SdkManifest


class ContentToolTest:
    REVISION = "a" * 64
    BUILD = 'Build it with "haylen sdk build --target host".'

    @staticmethod
    def sdk(folder: Path, configuration: str = "Release", revision: str = REVISION, system: str | None = None, tool: bool = True) -> SdkManifest:
        """Writes an SDK manifest whose host tools hold the content tool of this machine, or of another system."""
        path = f"bin/{Host.executable(ContentTool.NAME)}"
        document = {
            "format": 1,
            "port": "desktop",
            "portVersion": "1.0.0",
            "engineVersion": "0.0.1",
            "revision": revision,
            "configuration": configuration,
            "slices": [
                {
                    "system": Host.system(),
                    "environment": "host",
                    "architectures": [Host.architecture()],
                    "abi": "host",
                    "toolchain": "host",
                    "runtime": "host",
                    "minimumSystemVersion": "1.0",
                    "graphicsApis": [],
                    "capabilities": [],
                }
            ],
            "hostTools": [{"name": ContentTool.NAME, "system": system or Host.system(), "architecture": Host.architecture(), "path": path}],
        }
        folder.mkdir(parents=True)
        (folder / SdkManifest.FILE).write_text(json.dumps(document), encoding="utf-8")
        if tool:
            (folder / path).parent.mkdir()
            (folder / path).write_bytes(b"")
        return SdkManifest.load(folder)

    def test_finds_the_tool_of_this_machine_in_a_current_sdk_and_prefers_a_release_sdk(self, tmp_path):
        debug = ContentToolTest.sdk(tmp_path / "debug", "Debug")
        release = ContentToolTest.sdk(tmp_path / "release")

        assert ContentTool.find([debug, release], ContentToolTest.REVISION, ContentToolTest.BUILD) == release.folder / "bin" / Host.executable(ContentTool.NAME)
        assert ContentTool.find([debug], ContentToolTest.REVISION, ContentToolTest.BUILD) == debug.folder / "bin" / Host.executable(ContentTool.NAME)

    def test_never_runs_a_tool_of_other_sources_another_system_or_a_missing_file(self, tmp_path):
        stale = ContentToolTest.sdk(tmp_path / "stale", revision="b" * 64)
        foreign = ContentToolTest.sdk(tmp_path / "foreign", system="otheros")
        incomplete = ContentToolTest.sdk(tmp_path / "incomplete", tool=False)

        with pytest.raises(IncompatibleSdk) as raised:
            ContentTool.find([stale, foreign, incomplete], ContentToolTest.REVISION, ContentToolTest.BUILD)

        message = raised.value.message
        assert f'No installed SDK holds the content tool "{ContentTool.NAME}" for "{Host.system()}"' in message
        assert ContentToolTest.BUILD in message
        assert "built from other engine sources" in message
        assert "which does not exist" in message
        assert "foreign" not in message
