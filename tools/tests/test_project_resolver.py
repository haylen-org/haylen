import re

import pytest

from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from tests.support.tooling_fixture import ToolingFixture


class ProjectResolverTest:
    def test_resolves_one_project_per_architecture_with_its_own_identity(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({})

        default = fixture.resolve(app)
        both = fixture.resolve(app, architectures=("arm64", "x86_64"))

        assert default.identity == "fake-debug"
        assert both.identity == "fake-debug-arm64-x86_64"
        assert [project.target.architecture for project in both.projects] == ["arm64", "x86_64"]
        assert [project.target.abi for project in both.projects] == ["fake-arm64", "fake-x86_64"]
        assert both.backend == "fake"

    def test_lua_and_cpp_apps_get_the_same_resolution(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        plugin = {
            "implementations": {"fake": "native"},
            "contributions": [{"type": "package", "platforms": ["fake"], "url": "https://example.com/kit", "version": "1"}],
        }
        lua = fixture.app({"plugins": {"kit": {}}}, "lua")
        cpp = fixture.app({"kind": "cpp", "cpp": {"sources": ["source"]}, "plugins": {"kit": {}}}, "cpp")
        fixture.plugin(lua, "kit", plugin)
        fixture.plugin(cpp, "kit", plugin)

        lua_project = fixture.resolve(lua).primary
        cpp_project = fixture.resolve(cpp).primary

        assert [contribution.describe()["fields"] for contribution in lua_project.contributions] == [contribution.describe()["fields"] for contribution in cpp_project.contributions]
        assert lua_project.target == cpp_project.target
        assert lua_project.sdk == cpp_project.sdk

    def test_orders_plugins_after_their_dependencies(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"analytics": {}, "cloud": {}}})
        fixture.plugin(app, "analytics", {"dependencies": {"cloud": "1.0"}})
        fixture.plugin(app, "cloud", {"version": "1.4.0"})

        plugins = fixture.resolve(app).plugins

        assert [plugin.id for plugin in plugins] == ["cloud", "analytics"]

    def test_refuses_cycles_missing_dependencies_and_versions(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"a": {}, "b": {}}})
        fixture.plugin(app, "a", {"dependencies": {"b": "1.0"}})
        fixture.plugin(app, "b", {"dependencies": {"a": "1.0"}})
        with pytest.raises(InvalidManifest, match=re.escape('The plugins "a", "b" depend on each other in a cycle.')):
            fixture.resolve(app)

        other = fixture.app({"plugins": {"c": {}, "d": {}}}, "other")
        fixture.plugin(other, "c", {"dependencies": {"d": "2.0", "e": "1.0"}})
        fixture.plugin(other, "d", {"version": "1.5.0"})
        with pytest.raises(InvalidManifest) as raised:
            fixture.resolve(other)
        assert 'The plugin "c" needs the plugin "d" 2.0.0 or a newer version of the same major version, and the app lists 1.5.0.' in raised.value.message
        assert 'The plugin "c" depends on the plugin "e", which' in raised.value.message

    def test_a_plugin_without_an_implementation_for_the_target_fails(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"web-only": {}}})
        fixture.plugin(app, "web-only", {"implementations": {"web": "lua"}})

        with pytest.raises(UnsupportedTarget, match=re.escape('The plugin "web-only" does not support "fake". It supports "web"')):
            fixture.resolve(app)

    def test_checks_parameters_and_substitutes_them(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        plugin = {
            "implementations": {"fake": "native"},
            "parameters": {
                "version": {"type": "string", "required": True, "description": "The version of the kit."},
                "flag": {"type": "boolean", "default": False, "description": "A flag."},
            },
            "contributions": [{"type": "package", "platforms": ["fake"], "url": "https://example.com/kit/${version}", "version": "${version}"}],
        }
        missing = fixture.app({"plugins": {"kit": {"parameters": {"other": 1}}}}, "missing")
        fixture.plugin(missing, "kit", plugin)
        with pytest.raises(UnsupportedTarget) as raised:
            fixture.resolve(missing)
        assert 'The key "plugins.kit.parameters.other" is no parameter of the plugin "kit".' in raised.value.message
        assert 'The key "plugins.kit.parameters.version" needs a value to build for "fake". The version of the kit.' in raised.value.message

        given = fixture.app({"plugins": {"kit": {"parameters": {"version": "2.1"}}}}, "given")
        fixture.plugin(given, "kit", plugin)
        project = fixture.resolve(given).primary
        assert project.plugins[0].parameters == {"version": "2.1", "flag": False}
        assert dict(project.of_type("package")[0].fields) == {"url": "https://example.com/kit/2.1", "version": "2.1"}

    def test_refuses_contributions_that_no_backend_reads(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"kit": {}}})
        fixture.plugin(
            app,
            "kit",
            {
                "implementations": {"fake": "native"},
                "contributions": [{"type": "gradleModule", "platforms": ["fake"], "path": "android"}, {"type": "package", "platforms": ["fake"], "url": 3}],
            },
        )

        with pytest.raises(UnsupportedTarget) as raised:
            fixture.resolve(app)

        assert 'The contribution "gradleModule" at' in raised.value.message
        assert 'is of a type that no backend of "fake" reads.' in raised.value.message
        assert 'The key "url" must be a text.' in raised.value.message
        assert 'The file needs "version".' in raised.value.message

    def test_merges_the_settings_of_the_app_with_the_plugins_and_names_conflicts(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        plugin = {
            "implementations": {"fake": "native"},
            "contributions": [{"type": "settings", "platforms": ["fake"], "values": {"schemes": ["kit"], "color": "red"}}],
        }
        agreeing = fixture.app({"plugins": {"kit": {}}, "platforms": {"fake": {"settings": {"schemes": ["app"], "title": "App"}}}}, "agreeing")
        fixture.plugin(agreeing, "kit", plugin)

        merged = fixture.resolve(agreeing).primary.of_type("settings")[0]

        assert dict(merged.fields["values"]) == {"schemes": ("kit", "app"), "color": "red", "title": "App"}
        assert merged.owners == ("kit", "app")

        conflicting = fixture.app({"plugins": {"kit": {}}, "platforms": {"fake": {"settings": {"color": "blue"}}}}, "conflicting")
        fixture.plugin(conflicting, "kit", plugin)
        with pytest.raises(InvalidContribution, match=re.escape('The key "settings.values.color" is "red" for "kit" and "blue" for "app"')):
            fixture.resolve(conflicting)

    def test_two_owners_of_one_identity_must_agree(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"a": {}, "b": {}}})
        for identifier, version in (("a", "1"), ("b", "2")):
            fixture.plugin(
                app,
                identifier,
                {
                    "implementations": {"fake": "native"},
                    "contributions": [{"type": "package", "platforms": ["fake"], "url": "https://example.com/kit", "version": version}],
                },
            )

        with pytest.raises(InvalidContribution, match=re.escape('The contribution "package" with "url" "https://example.com/kit" differs in "version" between "a"')):
            fixture.resolve(app)

    def test_two_owners_of_one_identity_join_the_lists_that_its_type_joins(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"a": {}, "b": {}}})
        for identifier, products in (("a", ["Core", "Ads"]), ("b", ["Core", "Analytics"])):
            contribution = {"type": "package", "platforms": ["fake"], "url": "https://example.com/kit", "version": "2.0", "products": products}
            fixture.plugin(app, identifier, {"implementations": {"fake": "native"}, "contributions": [contribution]})

        merged = fixture.resolve(app).primary.of_type("package")
        assert len(merged) == 1
        assert merged[0].owners == ("a", "b")
        assert merged[0].fields["products"] == ("Core", "Ads", "Analytics")
        assert merged[0].fields["version"] == "2.0"

    def test_one_owner_of_an_exclusive_type(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"plugins": {"a": {}, "b": {}}})
        for identifier in ("a", "b"):
            fixture.plugin(app, identifier, {"implementations": {"fake": "native"}, "contributions": [{"type": "packs", "platforms": ["fake"], "packs": [identifier]}]})

        with pytest.raises(InvalidContribution, match=re.escape('The contribution "packs" comes from one owner of an app')):
            fixture.resolve(app)

    def test_settles_the_oldest_system_version(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk(minimum="2.0")
        plugin = {"implementations": {"fake": "native"}, "contributions": [{"type": "minimumSystemVersion", "platforms": ["fake"], "version": "3.1"}]}
        plain = fixture.app({}, "plain")
        assert fixture.resolve(plain).primary.minimum_system_version.value == "2.0"

        raised_by_plugin = fixture.app({"plugins": {"kit": {}}}, "raised")
        fixture.plugin(raised_by_plugin, "kit", plugin)
        minimum = fixture.resolve(raised_by_plugin).primary.minimum_system_version
        assert (minimum.value, minimum.justification) == ("3.1", 'The plugin "kit" needs it, and the app sets none.')

        fixed = fixture.app({"plugins": {"kit": {}}, "platforms": {"fake": {"minimumSystemVersion": "3.0"}}}, "fixed")
        fixture.plugin(fixed, "kit", plugin)
        with pytest.raises(UnsupportedTarget, match=r'The app runs on "fake" 3.0 and newer, .* while the plugin "kit" needs 3.1.'):
            fixture.resolve(fixed)

    def test_an_sdk_too_new_for_the_app_does_not_serve_it(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk(minimum="5.0")
        app = fixture.app({"platforms": {"fake": {"minimumSystemVersion": "4.0"}}})
        with pytest.raises(IncompatibleSdk, match=re.escape('runs on "fakeos" 5.0 and newer, while the app supports 4.0.')):
            fixture.resolve(app)

    def test_names_why_no_sdk_serves(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        with pytest.raises(
            IncompatibleSdk,
            match=re.escape('No installed SDK holds the engine for "fake" on "arm64". Build it with "haylen sdk build --target fake --config Release".'),
        ):
            fixture.resolve(app)

        fixture.sdk(configuration="Debug")
        fixture.sdk(revision="b" * 64)
        with pytest.raises(IncompatibleSdk) as raised:
            fixture.resolve(app)
        assert 'is built in the "Debug" configuration, not in "Release".' in raised.value.message
        assert "is built from other engine sources than the ones of this checkout." in raised.value.message

    def test_an_sdk_without_the_capabilities_of_the_backend_does_not_serve(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk(capabilities=())
        with pytest.raises(UnsupportedTarget, match=re.escape('lacks the capabilities "dynamicLibraries"')):
            fixture.resolve(fixture.app({}))

    def test_refuses_sections_that_no_backend_reads(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"platforms": {"console": {}, "fake": {"signing": 3, "files": {"../outside": "source"}}}})
        with pytest.raises(InvalidManifest) as raised:
            fixture.resolve(app)
        assert 'The section "platforms.console" is read by no installed backend. The sections are "fake".' in raised.value.message
        assert 'In "platforms.fake": The key "signing" must be a text.' in raised.value.message
        assert 'The key "platforms.fake.files.../outside" must map a path inside the generated project' in raised.value.message

    def test_unknown_targets_and_architectures(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({})
        with pytest.raises(UnsupportedTarget, match=re.escape('No installed backend builds the target "moon". The targets are "fake", "fake-simulator"')):
            fixture.resolve(app, target="moon")
        with pytest.raises(UnsupportedTarget, match=re.escape('builds for "arm64" or "x86_64", not for "riscv64"')):
            fixture.resolve(app, architectures=("riscv64",))

    def test_the_resolution_is_immutable(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        fixture.sdk()
        app = fixture.app({"platforms": {"fake": {"settings": {"title": "App"}}}})
        project = fixture.resolve(app).primary
        with pytest.raises(TypeError):
            project.settings["fake"]["signing"] = "key"
        with pytest.raises(AttributeError):
            project.platform = "other"
