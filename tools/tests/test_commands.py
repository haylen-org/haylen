import json
import re
from dataclasses import replace
from pathlib import Path
from types import MappingProxyType

import pytest

from haylen.cli.application import Application
from haylen.cli.arguments import Arguments
from haylen.cli.command_session import CommandSession
from haylen.cli.commands.doctor import DoctorCommand
from haylen.content.content_tool import ContentTool
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.missing_capability import MissingCapability
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.capability import Capability
from haylen.platforms.device import Device
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.sdk_request import SdkRequest
from haylen.plugins.plugin_lock import PluginLock
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.terminal import Terminal
from tests.content.test_content_tool import ContentToolTest
from tests.support.fake_backend import FakeBackend
from tests.support.fake_port import FakePort
from tests.support.tooling_fixture import ToolingFixture


class CommandsTest:
    """The commands run through the parser of the command line against the fake backend, whose SDKs and plugins come from the fixture."""

    class DeviceBackend(FakeBackend):
        """The fake backend with the devices of several platforms, in the order a backend reports them."""

        DEVICES = (
            Device("SIM-OLD", "iPhone 17", "simulator", "iOS", "26.0", "shutdown", ("ios-simulator",)),
            Device("SIM-NEW", "iPad (A16)", "simulator", "iPadOS", "27.2", "booted", ("ios-simulator",)),
            Device("00008110-001428CC0CD0401E", "My iPhone", "device", "iOS", "27.2.1", "disconnected", ("ios",)),
            Device("emulator-5554", "Pixel_10a", "emulator", "Android", "17", "ready", ("android",)),
            Device("CONSOLE", "Dev kit", "device", "fakeos", "", "ready", ("fake",)),
        )

        def describe(self) -> PlatformDescription:
            described = super().describe()
            return replace(described, capabilities=described.capabilities | {Capability.DEVICES})

        def devices(self) -> tuple[Device, ...]:
            return CommandsTest.DeviceBackend.DEVICES

    @pytest.fixture(autouse=True)
    def own_sdks(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        """Every test reads the SDKs of its own folder, never those that the checkout installed."""
        monkeypatch.setattr(Repository, "SDKS", tmp_path / "sdk")

    @staticmethod
    def session(fixture: ToolingFixture, monkeypatch: pytest.MonkeyPatch, structured: bool = False) -> CommandSession:
        session = CommandSession(fixture.context, structured)
        session.registry = fixture.registry()

        def resolve(pipeline: AppPipeline, request: ProductRequest, *, build_sdk: bool) -> ResolvedProduct:
            return fixture.resolver(request.app_folder, pipeline.registry).resolve(request)

        monkeypatch.setattr(AppPipeline, "resolve", resolve)
        return session

    @staticmethod
    def run(session: CommandSession, words: list[str]) -> object:
        parser, commands = Application.parser()
        arguments = parser.parse_args(words)
        return commands[arguments.command_path].run(arguments, session)

    @pytest.mark.parametrize(
        ("words", "capability"),
        [(["app", "package", "--target", "fake", "--format", "store"], "package"), (["app", "run", "--target", "fake"], "run")],
    )
    def test_a_backend_without_the_capability_stops_the_command_before_it_prepares_anything(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, words: list[str], capability: str):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({})
        session = CommandsTest.session(fixture, monkeypatch)
        with pytest.raises(MissingCapability, match=re.escape(f'The backend "fake" of "fake" does not {capability} apps.')):
            CommandsTest.run(session, [*words[:2], str(app), *words[2:]])

    def test_the_release_that_a_build_follows_needs_the_release_configuration(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        shipped = tmp_path / "shipped"
        shipped.mkdir()
        session = CommandsTest.session(fixture, monkeypatch)
        parser, _ = Application.parser()

        debug = parser.parse_args(["app", "build", str(app), "--target", "fake", "--previous-release", str(shipped)])
        with pytest.raises(InvalidOption, match=re.escape('takes "--config Release"')):
            Arguments.request(debug, session, debug.previous_release)

        release = parser.parse_args(["app", "build", str(app), "--target", "fake", "--config", "Release", "--previous-release", str(shipped)])
        assert Arguments.request(release, session, release.previous_release).previous_release == shipped.resolve()

    def test_a_new_app_names_a_category_and_an_identifier_only_when_the_command_gives_them(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        session = CommandsTest.session(ToolingFixture(tmp_path), monkeypatch)
        notes = CommandsTest.run(session, ["new", str(tmp_path / "notes")])
        puzzle = CommandsTest.run(session, ["new", str(tmp_path / "puzzle"), "--category", "game", "--identifier", "com.example.puzzle"])

        assert (notes["category"], notes["identifier"], puzzle["category"], puzzle["identifier"]) == (None, None, "game", "com.example.puzzle")
        plain = json.loads((tmp_path / "notes" / "haylen.json").read_text(encoding="utf-8"))
        assert "category" not in plain and "identifier" not in plain
        assert plain["platforms"]["android"]["applicationClass"] == "app.App"
        assert (tmp_path / "notes" / "native" / "android" / "App.kt").read_text(encoding="utf-8").startswith("package app\n")
        named = json.loads((tmp_path / "puzzle" / "haylen.json").read_text(encoding="utf-8"))
        assert list(named)[:6] == ["name", "identifier", "version", "build", "category", "description"]
        assert named["platforms"]["android"]["applicationClass"] == "com.example.puzzle.App"

    def test_content_publishes_to_the_channel_that_the_engine_follows_by_default(self):
        parser, _ = Application.parser()
        assert parser.parse_args(["content", "publish", "--target", "web", "--output", "tree"]).channel == "stable"
        assert parser.parse_args(["content", "publish", "--target", "web", "--output", "tree", "--channel", "beta"]).channel == "beta"

    def test_the_plugin_list_names_the_plugin_folders_that_the_app_does_not_list(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({"plugins": {"kit": {}}})
        fixture.plugin(app, "kit", {})
        fixture.plugin(app, "stray", {})

        listed = CommandsTest.run(CommandsTest.session(fixture, monkeypatch), ["plugin", "list", "--app", str(app)])
        assert [(plugin["id"], plugin["listed"]) for plugin in listed["plugins"]] == [("kit", True), ("stray", False)]
        lines = fixture.output.getvalue().splitlines()
        assert lines[:2] == ["Plugins  2", "  Plugin  Version  Platforms  Source         State"]
        assert lines[3] == "  stray   1.0.0    fake       plugins/stray  \u25cb Not listed"

    def test_devices_print_a_group_of_every_platform_in_the_order_developers_look_for_them(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        session = CommandsTest.session(fixture, monkeypatch)
        session.registry = fixture.registry([CommandsTest.DeviceBackend])
        CommandsTest.run(session, ["devices"])

        assert fixture.output.getvalue().splitlines() == [
            "Android  1",
            "  Name        Kind       System       State           Identifier",
            "  Pixel_10a   Emulator   Android 17   \u25cf Ready         emulator-5554",
            "",
            "iOS and iPadOS  3",
            "  Name        Kind       System       State           Identifier",
            "  My iPhone   Device     iOS 27.2.1   \u00d7 Disconnected  00008110-001428CC0CD0401E",
            "  iPad (A16)  Simulator  iPadOS 27.2  \u25cf Booted        SIM-NEW",
            "  iPhone 17   Simulator  iOS 26.0     \u25cb Shutdown      SIM-OLD",
            "",
            "fakeos  1",
            "  Name        Kind       System       State           Identifier",
            "  Dev kit     Device     fakeos       \u25cf Ready         CONSOLE",
        ]

    def test_the_json_of_devices_holds_every_field_of_every_device_and_the_terminal_prints_nothing(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        session = CommandsTest.session(fixture, monkeypatch, structured=True)
        session.registry = fixture.registry([CommandsTest.DeviceBackend])
        listed = CommandsTest.run(session, ["devices"])

        assert listed == {"devices": [{"backend": "fake", **device.describe()} for device in CommandsTest.DeviceBackend.DEVICES], "problems": []}
        assert listed["devices"][0] == {
            "backend": "fake",
            "identifier": "SIM-OLD",
            "name": "iPhone 17",
            "kind": "simulator",
            "system": "iOS",
            "version": "26.0",
            "state": "shutdown",
            "targets": ["ios-simulator"],
        }
        assert fixture.output.getvalue() == ""

    def test_the_doctor_sums_up_the_targets_in_a_table_and_lists_every_problem_once(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        CommandsTest.run(CommandsTest.session(fixture, monkeypatch), ["doctor"])
        lines = fixture.output.getvalue().splitlines()

        assert lines[0] == f"Haylen {Repository.engine_version()} on {Host.system()} {Host.architecture()}"
        assert lines[2:6] == [
            "Targets  2",
            "  Target          Backend  Engine port  SDK  State",
            "  fake            fake     \u2014            \u2014    \u2717 Problems",
            "  fake-simulator  fake     \u2014            \u2014    \u2717 Problems",
        ]
        assert lines[7] == "Problems  2"
        assert lines[8].startswith('  \u2717 No installed engine port builds the engine for "fake"')

    def test_the_json_of_the_doctor_keeps_its_fields(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        fixture.sdk()
        described = CommandsTest.run(CommandsTest.session(fixture, monkeypatch, structured=True), ["doctor", "--target", "fake"])

        target = described["targets"][0]
        assert list(described) == ["host", "problems", "targets"]
        assert list(target) == ["target", "backend", "capabilities", "port", "sdk", "tools", "problems", "ready"]
        assert target["port"].startswith('No installed engine port builds the engine for "fake"')
        assert target["sdk"].endswith("(Release)")
        assert (target["tools"], target["problems"], target["ready"]) == ([], [], True)
        assert fixture.output.getvalue() == ""

    def test_inspect_shows_the_settings_of_the_target_with_their_origins(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"platforms": {"fake": {"signing": "team", "minimumSystemVersion": "2.0"}}})
        origin = f"{fixture.resolve(app).app.origin} at platforms.fake.signing"

        CommandsTest.run(CommandsTest.session(fixture, monkeypatch), ["project", "inspect", str(app), "--target", "fake"])
        lines = fixture.output.getvalue().splitlines()
        settings = lines.index("Settings  1")
        assert re.fullmatch(r"  Setting +Value +Origin", lines[settings + 1])
        assert re.fullmatch(rf'  platforms\.fake\.signing +"team" +{re.escape(fixture.resolve(app).app.origin)}', lines[settings + 2])

        described = CommandsTest.run(CommandsTest.session(fixture, monkeypatch, structured=True), ["project", "inspect", str(app), "--target", "fake"])
        assert described["targets"][0]["settings"] == {"fake": {"signing": {"value": "team", "origin": origin}}}

    def test_the_doctor_says_that_a_pinned_tool_not_downloaded_yet_comes_with_the_first_build(self):
        missing = {"name": "Gradle", "version": "9.8.2", "path": None, "problem": None}
        found = {"name": "cmake", "version": "cmake version 4.4.3", "path": "/opt/homebrew/bin/cmake", "problem": None}
        broken = {"name": "Xcode", "version": None, "path": None, "problem": 'The command "xcodebuild" does not run.'}
        assert [DoctorCommand.version(tool) for tool in (missing, found, broken)] == ["9.8.2", "4.4.3", ""]
        assert [DoctorCommand.tool_path(tool) for tool in (missing, found, broken)] == ["Downloads with the first build", "/opt/homebrew/bin/cmake", ""]
        assert [DoctorCommand.tool_cell(tool).sign for tool in (missing, found, broken)] == [Terminal.Sign.IDLE, Terminal.Sign.READY, Terminal.Sign.FAILED]

    def test_an_app_build_builds_the_sdk_that_no_installed_sdk_of_the_sources_holds_while_an_inspection_never_builds(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        fixture.sdk(revision="b" * 64)
        requests: list[SdkRequest] = []

        class BuildingPort(FakePort):
            def build_sdk(self, request: SdkRequest) -> SdkManifest:
                requests.append(request)
                return fixture.sdk(architectures=request.architectures)

        session = CommandSession(fixture.context, False)
        session.registry = BackendRegistry(fixture.context, [("FakeBackend", lambda: FakeBackend)], [("BuildingPort", lambda: BuildingPort)])
        request = ProductRequest(fixture.app({}), "fake", Configuration.DEBUG, Configuration.RELEASE, ("arm64",))
        with pytest.raises(IncompatibleSdk, match="other engine sources"):
            session.pipeline.resolve(request, build_sdk=False)

        resolved = session.pipeline.resolve(request, build_sdk=True)
        session.pipeline.resolve(request, build_sdk=True)
        assert [(built.targets, built.architectures, built.configuration) for built in requests] == [(("fake-device",), ("arm64",), Configuration.RELEASE)]
        assert resolved.primary.sdk.manifest.revision == ToolingFixture.REVISION
        assert AppPipeline.SDK_BUILD_NOTE in fixture.output.getvalue()

        # A project that fails to resolve, such as one whose plugin does not support the target, fails before any SDK builds.
        broken = fixture.app({"plugins": {"kit": {}}}, name="broken")
        fixture.plugin(broken, "kit", {"implementations": {"elsewhere": "lua"}})
        with pytest.raises(UnsupportedTarget, match='The plugin "kit" does not support "fake"'):
            session.pipeline.resolve(ProductRequest(broken, "fake", Configuration.DEBUG, Configuration.RELEASE, ("x86_64",)), build_sdk=True)
        assert len(requests) == 1

    @staticmethod
    def building_session(fixture: ToolingFixture, requests: list[SdkRequest]) -> CommandSession:
        """A session whose engine port records the SDKs it builds and installs them in the fixture."""

        class BuildingPort(FakePort):
            def build_sdk(self, request: SdkRequest) -> SdkManifest:
                requests.append(request)
                return fixture.sdk(architectures=request.architectures)

        session = CommandSession(fixture.context, False)
        session.registry = BackendRegistry(fixture.context, [("FakeBackend", lambda: FakeBackend)], [("BuildingPort", lambda: BuildingPort)])
        return session

    def test_a_build_never_stops_on_or_names_an_sdk_of_other_targets_that_this_tooling_cannot_use(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        fixture.unusable_sdk("other-older", system="otheros")
        fixture.unusable_sdk("other-damaged", system="otheros", damaged=True)
        fixture.sdk()
        requests: list[SdkRequest] = []
        session = CommandsTest.building_session(fixture, requests)

        resolved = session.pipeline.resolve(ProductRequest(fixture.app({}), "fake", Configuration.DEBUG, Configuration.RELEASE, ("arm64",)), build_sdk=True)

        assert resolved.primary.sdk.manifest.revision == ToolingFixture.REVISION
        assert requests == []
        assert "other-" not in fixture.output.getvalue()

    def test_an_sdk_of_the_target_that_an_older_haylen_built_is_built_again_and_an_inspection_says_how(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        fixture.unusable_sdk("fake-release")
        requests: list[SdkRequest] = []
        session = CommandsTest.building_session(fixture, requests)
        request = ProductRequest(fixture.app({}), "fake", Configuration.DEBUG, Configuration.RELEASE, ("arm64",))

        with pytest.raises(IncompatibleSdk, match=re.escape('No installed SDK holds the engine for "fake" on "arm64". Build it with "haylen sdk build --target fake --config Release".')):
            session.pipeline.resolve(request, build_sdk=False)
        session.pipeline.resolve(request, build_sdk=True)

        assert [(built.targets, built.architectures) for built in requests] == [(("fake-device",), ("arm64",))]
        assert AppPipeline.SDK_BUILD_NOTE in fixture.output.getvalue()
        assert not re.search(r"\[\d+\]|libraries", fixture.output.getvalue())

    def test_a_record_that_an_older_haylen_wrote_is_prepared_again_or_named_in_plain_words(self, tmp_path: Path):
        (tmp_path / PreparationState.FILE).write_text(json.dumps({"generator": "0.0.1"}), encoding="utf-8")
        assert PreparationState.read(tmp_path) is None

        (tmp_path / PluginLock.FILE).write_text(json.dumps({"plugins": []}), encoding="utf-8")
        with pytest.raises(InvalidManifest) as raised:
            PluginLock.load(tmp_path)
        assert raised.value.message.endswith('is damaged or an older version of Haylen wrote it. Write it again with "haylen plugin update".')
        assert "{" not in raised.value.message

    def test_the_doctor_and_the_sdk_list_show_an_sdk_that_an_older_haylen_built_and_a_damaged_one_in_plain_words(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        older = fixture.unusable_sdk("fake-release")
        damaged = fixture.unusable_sdk("web-release", damaged=True)
        rebuild = f'The SDK of "fake" in {older} was built by an older version of Haylen. Build it again with "haylen sdk build --target fake".'

        CommandsTest.run(CommandsTest.session(fixture, monkeypatch), ["doctor"])
        lines = fixture.output.getvalue().splitlines()
        assert re.fullmatch("  fake +fake +— +— +\u00d7 Built by an older Haylen", lines[4])
        assert f"  ✗ {rebuild}" in lines
        assert f'  ✗ The SDK in {damaged} cannot be read, since its file "haylen-sdk.json" is damaged. Build it again with "haylen sdk build --target <target>".' in lines
        described = CommandsTest.run(CommandsTest.session(fixture, monkeypatch, structured=True), ["doctor", "--target", "fake"])
        assert described["targets"][0]["sdk"] == rebuild.replace(str(older), f"`{older}`")

        fixture.output.seek(0)
        fixture.output.truncate()
        CommandsTest.run(CommandsTest.session(fixture, monkeypatch), ["sdk", "list"])
        printed = fixture.output.getvalue()
        # The doctor printed before, so the first group starts after a blank line.
        lines = printed.splitlines()[1:]
        assert lines[0] == "SDKs  2"
        assert re.fullmatch(rf"  {re.escape(str(older))} +— +— +— +\u00d7 Built by an older Haylen", lines[2])
        assert re.fullmatch(rf"  {re.escape(str(damaged))} +— +— +— +\u00d7 Unreadable", lines[3])
        assert f"  \u00d7 {rebuild}" in lines
        assert not re.search(r"\[\d+\]|libraries", printed)
        listed = CommandsTest.run(CommandsTest.session(fixture, monkeypatch, structured=True), ["sdk", "list"])
        assert listed["sdks"] == []
        assert [(entry["folder"], entry["readable"]) for entry in listed["unusable"]] == [(str(older.resolve()), True), (str(damaged.resolve()), False)]

    def test_a_release_build_builds_the_sdk_of_this_machine_that_carries_the_content_tool_when_none_holds_it(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "engine_revision", staticmethod(lambda: ToolingFixture.REVISION))
        fixture.sdk()
        ContentToolTest.sdk(tmp_path / "sdk" / "stale", revision="b" * 64)
        abis = MappingProxyType({Host.architecture(): "host"})
        requests: list[SdkRequest] = []

        class HostBackend(FakeBackend):
            def describe(self) -> PlatformDescription:
                description = super().describe()
                host = replace(FakeBackend.target("fake-host", TargetEnvironment.HOST, dict(abis), (Host.architecture(),)), system=Host.system())
                return replace(description, targets=(*description.targets, host))

        class HostPort(FakePort):
            def describe(self) -> PortDescription:
                target = PortTarget("fake-host-port", Host.system(), TargetEnvironment.HOST, abis, "fakecc", "libfake", "1.0", (), (), self.CAPABILITIES)
                return PortDescription("fake-host", 1, "1.0.0", (target,), (), (ContentTool.NAME,))

            def build_sdk(self, request: SdkRequest) -> SdkManifest:
                requests.append(request)
                return ContentToolTest.sdk(tmp_path / "sdk" / "host")

        session = CommandSession(fixture.context, False)
        session.registry = BackendRegistry(fixture.context, [("HostBackend", lambda: HostBackend)], [("FakePort", lambda: FakePort), ("HostPort", lambda: HostPort)])
        app = fixture.app({})
        session.pipeline.resolve(ProductRequest(app, "fake", Configuration.DEBUG, Configuration.RELEASE, ("arm64",)), build_sdk=True)
        session.pipeline.resolve(ProductRequest(app, "fake", Configuration.RELEASE, Configuration.RELEASE, ("arm64",)), build_sdk=False)
        assert requests == []

        release = ProductRequest(app, "fake", Configuration.RELEASE, Configuration.RELEASE, ("arm64",))
        session.pipeline.resolve(release, build_sdk=True)
        session.pipeline.resolve(release, build_sdk=True)
        assert [(built.targets, built.architectures, built.configuration) for built in requests] == [(("fake-host-port",), None, Configuration.RELEASE)]
        assert AppPipeline.HOST_TOOLS_NOTE in fixture.output.getvalue()
        assert ContentTool.locate(fixture.context, session.registry).executable == tmp_path / "sdk" / "host" / "bin" / Host.executable(ContentTool.NAME)

    def test_sdks_join_only_with_one_port_sources_and_configuration_whose_port_declares_joining(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        release = fixture.sdk()
        debug = fixture.sdk(configuration="Debug")
        session = CommandsTest.session(fixture, monkeypatch)
        session.registry = BackendRegistry(fixture.context, [("FakeBackend", lambda: FakeBackend)], [("FakePort", lambda: FakePort)])
        with pytest.raises(IncompatibleSdk, match="do not join"):
            CommandsTest.run(session, ["sdk", "join", str(release.folder), str(debug.folder)])
        with pytest.raises(MissingCapability, match='The engine port "fake" does not join SDKs'):
            CommandsTest.run(session, ["sdk", "join", str(release.folder), str(fixture.sdk(architectures=("arm64",)).folder)])
