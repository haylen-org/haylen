import shutil
from pathlib import Path

import pytest

from haylen.core.configuration import Configuration
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.player import player_backend
from haylen.platforms.player.player_backend import PlayerBackend
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.verification_check import VerificationCheck
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class PlayerBackendTest:
    """The player target stages the package of a Lua app with its plugins and runs it in the desktop player of the SDK of this machine, without compiling anything."""

    class RecordedRun:
        """Stands in for the run of the development tools and records how the backend starts the player."""

        commands: list[tuple[list, dict | None]] = []

        def __init__(self, context: object, product: object, launch: LaunchRequest, reach: object, shipped: Path) -> None:
            self.developing = launch.development
            self.arguments = ["--dev-server", "ws://127.0.0.1:8000/development"] if launch.development else []
            self.shipped = shipped

        def __enter__(self) -> PlayerBackendTest.RecordedRun:
            return self

        def __exit__(self, *_: object) -> None:
            pass

        def show(self, device: str) -> None:
            pass

        def follow(self, command: list, *, environment: dict | None = None) -> None:
            PlayerBackendTest.RecordedRun.commands.append(([str(part) for part in command], environment))

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    def app(self, fixture: DesktopFixture) -> Path:
        app = fixture.app({"plugins": {"helper": {}}}, "garden")
        fixture.plugin(app, "helper", {"macos": "lua", "windows": "lua", "linux": "lua"}, [])
        return app

    def test_the_preparation_stages_the_package_with_its_plugins_and_generates_nothing_else(self, fixture: DesktopFixture):
        product = fixture.resolve(self.app(fixture), "player")
        folders = DesktopFixture.folders(product)
        prepared = PlayerBackend(fixture.context).prepare(product, folders)

        assert (folders.package / "source" / "main.lua").is_file()
        assert (folders.package / "plugins" / "helper" / "plugin.json").is_file()
        assert [path.name for path in folders.project.iterdir()] == [PreparationState.FILE]
        assert prepared.project == folders.project
        assert not PlayerBackend(fixture.context).prepare(fixture.resolve(product.app.folder, "player"), folders).changed

    def test_the_player_has_no_release(self, fixture: DesktopFixture):
        product = fixture.resolve(self.app(fixture), "player", Configuration.RELEASE)
        with pytest.raises(UnsupportedTarget, match='so it has no "Release" configuration'):
            PlayerBackend(fixture.context).prepare(product, DesktopFixture.folders(product))

    def test_the_verification_finds_a_plugin_that_the_staged_package_lacks(self, fixture: DesktopFixture):
        backend = PlayerBackend(fixture.context)
        product = fixture.resolve(self.app(fixture), "player")
        built = backend.build(backend.prepare(product, DesktopFixture.folders(product)))
        assert backend.verify(built).passed

        shutil.rmtree(built.path / "plugins" / "helper")
        failures = backend.verify(built).failures
        assert [check.name for check in failures] == ["plugins"]
        assert failures[0].outcome is VerificationCheck.Outcome.FAILED

    def test_the_run_starts_the_player_of_the_sdk_in_development(self, fixture: DesktopFixture, monkeypatch: pytest.MonkeyPatch):
        monkeypatch.setattr(player_backend, "AppRun", PlayerBackendTest.RecordedRun)
        PlayerBackendTest.RecordedRun.commands = []
        backend = PlayerBackend(fixture.context)
        product = fixture.resolve(self.app(fixture), "player")
        built = backend.build(backend.prepare(product, DesktopFixture.folders(product)))
        backend.run(built, LaunchRequest(None, True, "127.0.0.1", 8000, 4711, {}))

        command, environment = PlayerBackendTest.RecordedRun.commands[0]
        assert command == [str(product.primary.sdk.file("player")), "--dev-server", "ws://127.0.0.1:8000/development", str(built.path)]
        assert environment == ({"MTL_CAPTURE_ENABLED": "1"} if Host.system() == "macos" else None)

        (PlayerBackend.native_folder(built.prepared.folders) / "libglue.so").write_bytes(b"glue")
        backend.run(built, LaunchRequest(None, True, "127.0.0.1", 8000, 4711, {}))
        assert PlayerBackendTest.RecordedRun.commands[1][0][3:5] == ["--native", str(PlayerBackend.native_folder(built.prepared.folders))]
