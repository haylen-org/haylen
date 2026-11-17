import re

from haylen.core.repository import Repository
from haylen.native.static_linking import StaticLinking
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.desktop.desktop_system import DesktopSystem


class PortContractTest:
    """The engine, its CMake and its tooling name the capabilities of a platform with the one table `port::Contract::kCapabilityNames`, which `haylen_port` reads too, so the capabilities that the engine ports of the tooling declare and that the backends need are always names that the engine knows."""

    CONTRACT = Repository.ENGINE / "include" / "haylen" / "port" / "Contract.hpp"

    @staticmethod
    def capability_names() -> frozenset[str]:
        text = PortContractTest.CONTRACT.read_text(encoding="utf-8")
        table = re.search(r"kCapabilityNames\{([^}]*)\}", text)
        return frozenset(re.findall(r'"([A-Za-z]+)"', table.group(1)))

    def test_the_tooling_names_only_capabilities_of_the_contract(self):
        declared = (
            DesktopSystem.CAPABILITIES
            | AndroidPlatform.CAPABILITIES
            | AppleTargets.MAC_CAPABILITIES
            | AppleTargets.CATALYST_CAPABILITIES
            | AppleTargets.MOBILE_CAPABILITIES
            | {StaticLinking.DYNAMIC_LIBRARIES}
        )
        assert sorted(declared - PortContractTest.capability_names()) == []
