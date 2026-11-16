import re

import pytest

from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.engine.consumers import Consumers
from haylen.platforms.apple.apple_backend import AppleBackend
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.player.player_backend import PlayerBackend
from haylen.sdk.sdk_catalog import SdkCatalog
from tests.platforms.apple.apple_fixture import AppleFixture
from tests.platforms.desktop.desktop_fixture import DesktopFixture
from tests.support.tooling_fixture import ToolingFixture


class ConsumersTest:
    def test_the_package_mode_takes_the_desktop_sdk_of_this_mac_and_never_the_apple_sdk(self, tmp_path, monkeypatch):
        monkeypatch.setattr(Host, "system", staticmethod(lambda: "macos"))
        monkeypatch.setattr(Host, "architecture", staticmethod(lambda: "arm64"))
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        apple = AppleFixture(tmp_path / "apple", monkeypatch)
        desktop = DesktopFixture(tmp_path / "desktop", monkeypatch)
        registry = BackendRegistry(apple.context, [("apple", lambda: AppleBackend), ("player", lambda: PlayerBackend)], [])
        consumers = Consumers(apple.context, registry)

        monkeypatch.setattr(SdkCatalog, "manifests", lambda catalog: [apple.sdk, desktop.sdks[0]])
        assert consumers.sdk(None, Configuration.RELEASE) == desktop.sdks[0].folder / desktop.sdks[0].slices[0].files["cmake"]

        monkeypatch.setattr(SdkCatalog, "manifests", lambda catalog: [apple.sdk])
        with pytest.raises(IncompatibleSdk, match=re.escape('Build it with "haylen sdk build --target player --config Release".')):
            consumers.sdk(None, Configuration.RELEASE)
