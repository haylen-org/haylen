from pathlib import Path
from types import MappingProxyType

import pytest

from haylen.core.configuration import Configuration
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.target_environment import TargetEnvironment
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.desktop.desktop_port import DesktopPort
from haylen.platforms.player.player_backend import PlayerBackend
from haylen.platforms.sdk_request import SdkRequest
from haylen.platforms.web.web_backend import WebBackend
from haylen.platforms.web.web_port import WebPort
from haylen.sdk.host_tool import HostTool
from haylen.sdk.sdk_folder import SdkFolder
from haylen.sdk.sdk_slice import SdkSlice
from tests.support.tooling_fixture import ToolingFixture


class DesktopPortTest:
    """The desktop and web engine ports build the SDKs that the desktop, player and web backends build apps against, matched by what their targets describe, and keep the slices of their folder that the same sources built."""

    @staticmethod
    def registry(fixture: ToolingFixture) -> BackendRegistry:
        backends = [(backend.NAME, lambda backend=backend: backend) for backend in (DesktopBackend, PlayerBackend, WebBackend)]
        ports = [(port.NAME, lambda port=port: port) for port in (DesktopPort, WebPort)]
        return BackendRegistry(fixture.context, backends, ports)

    def test_every_target_finds_its_engine_port_by_what_it_describes(self, tmp_path: Path):
        registry = DesktopPortTest.registry(ToolingFixture(tmp_path))
        registry.require_healthy()
        found = {target.description.name: registry.port_for(target.description) for target in registry.targets()}
        assert {name: (port.name, port_target.name) for name, (port, port_target) in found.items()} == {
            "windows": ("desktop", "desktop-windows"),
            "linux": ("desktop", "desktop-linux"),
            "player": ("desktop", f"desktop-{Host.system()}"),
            "web": ("web", "web"),
        }

    def test_the_desktop_port_builds_only_the_sdk_of_this_machine(self, tmp_path: Path):
        fixture = ToolingFixture(tmp_path)
        foreign = "windows" if Host.system() != "windows" else "linux"
        request = SdkRequest((f"desktop-{foreign}",), Configuration.RELEASE, None, ToolingFixture.REVISION, tmp_path / "sdk")
        with pytest.raises(UnsupportedTarget, match="builds only the SDK of this machine"):
            DesktopPort(fixture.context).build_sdk(request)
        report = DesktopPort(fixture.context).check_environment(f"desktop-{foreign}")
        assert any(f'only on a "{foreign}" machine' in problem for problem in report.problems)

    def test_the_desktop_port_of_windows_needs_perl_and_nasm_for_openssl(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        monkeypatch.setattr(Host, "system", staticmethod(lambda: "windows"))
        report = DesktopPort(ToolingFixture(tmp_path).context).check_environment("desktop-windows")
        assert [tool.name for tool in report.tools] == ["cmake", "perl", "nasm"]

    @staticmethod
    def slice_of(architecture: str) -> SdkSlice:
        return SdkSlice(
            "linux",
            TargetEnvironment.HOST,
            (architecture,),
            f"{architecture}-linux-gnu",
            "gnu",
            "libstdc++",
            "2.39",
            ("vulkan",),
            frozenset(),
            MappingProxyType({SdkSlice.ENGINE: "lib/libhaylen.a", SdkSlice.LUA: "lib/libhaylen-lua.a"}),
            MappingProxyType({}),
        )

    def test_a_folder_keeps_the_slices_of_the_same_sources_and_configuration(self, tmp_path: Path):
        folder = SdkFolder(tmp_path / "sdk", "desktop", "1.0.0", ToolingFixture.REVISION, Configuration.RELEASE)
        folder.begin()
        folder.slice_folder("linux-x86_64")
        folder.save(DesktopPortTest.slice_of("x86_64"), (HostTool("haylen-content", "linux", "x86_64", "linux-x86_64/bin/haylen-content"),))
        folder.begin()
        folder.slice_folder("linux-arm64")
        manifest = folder.save(DesktopPortTest.slice_of("arm64"), (HostTool("haylen-content", "linux", "arm64", "linux-arm64/bin/haylen-content"),))
        assert [entry.architectures for entry in manifest.slices] == [("x86_64",), ("arm64",)]
        assert [tool.architecture for tool in manifest.host_tools] == ["x86_64", "arm64"]

        rebuilt = folder.save(DesktopPortTest.slice_of("arm64"), ())
        assert [entry.architectures for entry in rebuilt.slices] == [("x86_64",), ("arm64",)]

        other = SdkFolder(tmp_path / "sdk", "desktop", "1.0.0", "b" * 64, Configuration.RELEASE)
        other.begin()
        assert not (tmp_path / "sdk" / "linux-x86_64").exists()
        assert [entry.architectures for entry in other.save(DesktopPortTest.slice_of("arm64"), ()).slices] == [("arm64",)]
