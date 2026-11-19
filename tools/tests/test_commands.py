import json
import re
from pathlib import Path

import pytest

from haylen.cli.application import Application
from haylen.cli.arguments import Arguments
from haylen.cli.command_session import CommandSession
from haylen.cli.commands.doctor import DoctorCommand
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.missing_capability import MissingCapability
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_manifest import SdkManifest
from tests.support.fake_backend import FakeBackend
from tests.support.fake_port import FakePort
from tests.support.tooling_fixture import ToolingFixture


class CommandsTest:
    """The commands run through the parser of the command line against the fake backend, whose SDKs and plugins come from the fixture."""

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

    def test_a_new_app_is_a_utility_unless_the_command_names_its_category(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        session = CommandsTest.session(ToolingFixture(tmp_path), monkeypatch)
        notes = CommandsTest.run(session, ["new", str(tmp_path / "notes")])
        puzzle = CommandsTest.run(session, ["new", str(tmp_path / "puzzle"), "--category", "game"])

        assert (notes["category"], puzzle["category"]) == ("utility", "game")
        assert json.loads((tmp_path / "notes" / "haylen.json").read_text(encoding="utf-8"))["category"] == "utility"
        assert json.loads((tmp_path / "puzzle" / "haylen.json").read_text(encoding="utf-8"))["category"] == "game"

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
        assert 'stray  1.0.0, fake, from plugins/stray, Not in "haylen.json", so no build carries it' in fixture.output.getvalue()

    def test_inspect_shows_the_settings_of_the_target_with_their_origins(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"platforms": {"fake": {"signing": "team", "minimumSystemVersion": "2.0"}}})
        origin = f"{fixture.resolve(app).app.origin} at platforms.fake.signing"

        CommandsTest.run(CommandsTest.session(fixture, monkeypatch), ["project", "inspect", str(app), "--target", "fake"])
        assert f'  "team" from {fixture.resolve(app).app.origin} at "platforms.fake.signing"' in fixture.output.getvalue()

        described = CommandsTest.run(CommandsTest.session(fixture, monkeypatch, structured=True), ["project", "inspect", str(app), "--target", "fake"])
        assert described["targets"][0]["settings"] == {"fake": {"signing": {"value": "team", "origin": origin}}}

    def test_the_doctor_says_that_a_pinned_tool_not_downloaded_yet_comes_with_the_first_build(self):
        missing = {"name": "Gradle", "version": "9.8.2", "path": None, "problem": None}
        found = {"name": "cmake", "version": "cmake version 4.4.3", "path": "/opt/homebrew/bin/cmake", "problem": None}
        broken = {"name": "Xcode", "version": None, "path": None, "problem": 'The command "xcodebuild" does not run.'}
        assert DoctorCommand.tool_state(missing) == "9.8.2, which the first build that needs it downloads into `.tools`"
        assert DoctorCommand.tool_state(found) == "cmake version 4.4.3 at `/opt/homebrew/bin/cmake`"
        assert DoctorCommand.tool_state(broken) == 'The command "xcodebuild" does not run.'

    def test_an_app_build_builds_the_sdk_that_no_installed_sdk_of_the_sources_holds_while_an_inspection_never_builds(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Repository, "SDKS", tmp_path / "sdk")
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
