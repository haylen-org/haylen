import re

import pytest

from haylen.core.app_kind import AppKind
from haylen.core.errors.incompatible_extension import IncompatibleExtension
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.target_environment import TargetEnvironment
from haylen.platforms.backend_registry import BackendRegistry
from tests.support.fake_backend import FakeBackend
from tests.support.fake_port import FakePort
from tests.support.tooling_fixture import ToolingFixture


class BackendRegistryTest:
    def test_finds_a_target_with_its_backend(self, tmp_path):
        registry = ToolingFixture(tmp_path).registry()
        target = registry.target("fake-simulator")
        assert target.description.environment is TargetEnvironment.SIMULATOR
        assert target.platform.name == "fake"

    def test_refuses_another_contract_version(self, tmp_path):
        registry = ToolingFixture(tmp_path).registry([FakeBackend.variant("OtherContractBackend", CONTRACT=99)])
        with pytest.raises(IncompatibleExtension, match=re.escape('The backend "OtherContractBackend" implements the backend contract 99, and this tooling implements 1.')):
            registry.target("fake")

    def test_an_extension_that_does_not_load_stops_the_commands_that_need_targets(self, tmp_path):
        registry = ToolingFixture(tmp_path).registry([FakeBackend, FakeBackend.variant("BrokenBackend", FAILURE="its module needs a newer tooling")])
        with pytest.raises(IncompatibleExtension, match=re.escape('The backend "BrokenBackend" does not load, since it is not a backend of Haylen or it fails as it starts.')):
            registry.target("fake")

    def test_refuses_two_backends_of_one_target(self, tmp_path):
        registry = ToolingFixture(tmp_path).registry([FakeBackend, FakeBackend.variant("Twin")])
        with pytest.raises(IncompatibleExtension, match=re.escape('The backends "FakeBackend" and "Twin" both declare the target "fake"')):
            registry.target("fake")

    def test_no_default_target_without_a_backend_that_marks_one(self, tmp_path):
        registry = ToolingFixture(tmp_path).registry()
        with pytest.raises(UnsupportedTarget, match=re.escape('No single target is the default of "lua" apps')):
            registry.default_target(AppKind.LUA)

    def test_matches_a_port_by_what_it_describes(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        registry = BackendRegistry(fixture.context, [("fake", lambda: FakeBackend)], [("fake", lambda: FakePort)])
        port, target = registry.port_for(registry.target("fake").description)
        assert target.name == "fake-device"
        assert isinstance(port.port, FakePort)

        with pytest.raises(UnsupportedTarget, match=re.escape('No installed engine port builds the engine for "fake-simulator"')):
            registry.port_for(registry.target("fake-simulator").description)

    def test_a_port_without_the_capabilities_of_the_backend_does_not_match(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        limited = FakePort.variant("LimitedPort", frozenset())
        registry = BackendRegistry(fixture.context, [("fake", lambda: FakeBackend)], [("limited", lambda: limited)])
        description = registry.target("fake").description
        assert BackendRegistry.mismatch(description, limited(fixture.context).describe().targets[0]) == 'It lacks the capabilities "dynamicLibraries" that the backend needs.'
        with pytest.raises(UnsupportedTarget, match=re.escape('No installed engine port builds the engine for "fake"')):
            registry.port_for(description)
