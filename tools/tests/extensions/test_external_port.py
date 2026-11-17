import json
import shutil
import subprocess
from pathlib import Path

import pytest

from haylen.core.host import Host
from haylen.core.repository import Repository
from tests.support.extension_environment import ExtensionEnvironment


@pytest.mark.port
class ExternalPortTest:
    """The proof that a platform joins Haylen from outside it. An engine port and a backend that are not part of the tooling, installed from copies outside the repository into an environment of their own, build the SDK of the engine, then prepare, build, verify and run a Lua app with a plugin and a file of content, without any change of the command line, the core or another backend, and every failure that such a platform meets stops with its typed error and its sentence. It builds the engine, so it runs only when asked for with `-m port`, as the job `port` of continuous integration does, with the jobs of `HAYLEN_JOBS`."""

    PORT = Repository.ENGINE / "tests" / "data" / "port"
    BACKEND = Repository.ROOT / "tools" / "tests" / "data" / "test-port-backend"
    APP = Repository.ROOT / "tools" / "tests" / "data" / "test-port-app"
    # The command line, the core and the internal backends, which no external platform may need to change, and the project that installs them.
    TOOLING = Repository.ROOT / "tools" / "haylen"
    PROJECT = Repository.ROOT / "pyproject.toml"
    TARGET = "test-port"
    PLUGIN = "test-port-greeter"
    CONFIGURATION = "Debug"
    # Ninja builds the engine on macOS and Linux, and Windows builds it with the generator of Visual Studio when Ninja is missing.
    TOOLS = ("uv", "cmake") if Host.system() == "windows" else ("uv", "cmake", "ninja")
    SDK_TIMEOUT = 3 * 3600
    BUILD_TIMEOUT = 1800
    IGNORED = shutil.ignore_patterns("__pycache__", "*.egg-info")

    @staticmethod
    def require(tools: tuple[str, ...]) -> None:
        missing = [tool for tool in tools if shutil.which(tool) is None]
        if missing:
            pytest.skip(f"The proof of an external port needs {', '.join(missing)} on the path, which this machine lacks.")

    @pytest.fixture(scope="class")
    @classmethod
    def tooling(cls) -> str:
        """The digest of the tooling before anything of the proof runs."""
        return ExtensionEnvironment.digest(ExternalPortTest.TOOLING)

    @pytest.fixture(scope="class")
    @classmethod
    def installed(cls, tooling: str, tmp_path_factory: pytest.TempPathFactory) -> ExtensionEnvironment:
        """The environment with the tooling of the checkout and the copies of the engine port, which reads its CMake port from its own folder and installs in editable mode, and of the backend, which installs as a wheel."""
        ExternalPortTest.require(ExternalPortTest.TOOLS)
        folder = tmp_path_factory.mktemp("external-port")
        port = shutil.copytree(ExternalPortTest.PORT, folder / "port", ignore=ExternalPortTest.IGNORED)
        backend = shutil.copytree(ExternalPortTest.BACKEND, folder / "backend", ignore=ExternalPortTest.IGNORED)
        environment = ExtensionEnvironment(folder / "environment")
        environment.create((port,), (backend,))
        return environment

    @pytest.fixture(scope="class")
    @classmethod
    def sdk(cls, installed: ExtensionEnvironment) -> dict:
        completed = installed.haylen("sdk", "build", "--target", ExternalPortTest.TARGET, "--config", ExternalPortTest.CONFIGURATION, "--json", timeout=ExternalPortTest.SDK_TIMEOUT, build=True)
        return ExtensionEnvironment.result(completed)

    def app(self, installed: ExtensionEnvironment, action: str, *arguments: object, environment: dict[str, str] | None = None) -> subprocess.CompletedProcess:
        """Runs an action of `haylen app` on the app of the proof for the target of the port, with the engine of the proof."""
        command = ("app", action, ExternalPortTest.APP, "--target", ExternalPortTest.TARGET, "--engine-config", ExternalPortTest.CONFIGURATION, *arguments)
        return installed.haylen(*command, environment=environment, timeout=ExternalPortTest.BUILD_TIMEOUT, build=action != "verify")

    def test_an_external_port_builds_verifies_and_runs_an_app_without_a_change_of_the_tooling(self, tooling: str, installed: ExtensionEnvironment, sdk: dict):
        doctor = ExtensionEnvironment.result(installed.haylen("doctor", "--target", ExternalPortTest.TARGET, "--json"))
        target = doctor["targets"][0]
        assert doctor["problems"] == []
        assert target["backend"] == "test-port"
        assert target["port"] == '"test-port" 1.0.0 builds the engine for it as "test-port-host".'

        installed_slice = sdk["slices"][0]
        assert sdk["port"] == "test-port"
        assert (installed_slice["system"], installed_slice["environment"], installed_slice["toolchain"]) == ("test-port", "host", "test-port-cc")
        assert installed_slice["capabilities"] == ["dynamicLibraries"]
        assert installed_slice["graphicsApis"] == ["port"]
        assert [tool["name"] for tool in sdk["hostTools"]] == ["port-shaders.cmake"]

        built = ExtensionEnvironment.result(self.app(installed, "build", "--json"))
        outcomes = {(check["source"], check["name"]): check["outcome"] for check in built["verification"]["checks"]}
        assert built["verification"]["passed"]
        assert outcomes[("core", "targets")] == "passed"
        assert outcomes[("test-port", "executable")] == "passed"
        assert outcomes[("test-port", "content")] == "passed"
        assert outcomes[("test-port", "parts")] == "passed"

        verified = ExtensionEnvironment.result(self.app(installed, "verify", "--json"))
        assert verified["verification"]["passed"]

        ran = self.app(installed, "run", "--port", "0", "--debugger-port", "0")
        assert ran.returncode == 0, ExtensionEnvironment.shown(ran)
        assert "The app of the test port passed every check" in ran.stdout, ExtensionEnvironment.shown(ran)

        assert ExtensionEnvironment.digest(ExternalPortTest.TOOLING) == tooling
        files = [path for path in (*ExternalPortTest.TOOLING.rglob("*"), ExternalPortTest.PROJECT) if path.is_file() and "__pycache__" not in path.parts]
        assert [path for path in files if b"test-port" in path.read_bytes()] == []

    def test_a_machine_without_the_tools_of_the_target_stops_before_anything_builds(self, installed: ExtensionEnvironment, sdk: dict):
        bare = {"PATH": str(installed.command.parent)}
        port = ExtensionEnvironment.error(installed.haylen("sdk", "build", "--target", ExternalPortTest.TARGET, "--config", ExternalPortTest.CONFIGURATION, "--json", environment=bare))
        assert port["code"] == "missingToolchain"
        assert port["message"].startswith('This machine lacks what the engine port "test-port" needs to build "test-port-host":\n  The command "cmake" was not found.')

        app = ExtensionEnvironment.error(self.app(installed, "build", "--json", environment=bare))
        assert app["code"] == "missingToolchain"
        assert app["message"].startswith('This machine lacks what "test-port" needs. The command "haylen doctor --target test-port" shows it too:\n  The command "cmake" was not found.')

    def test_a_backend_of_another_contract_version_stops_every_command_that_needs_it(self, tmp_path: Path):
        ExternalPortTest.require(("uv",))
        variant = shutil.copytree(ExternalPortTest.BACKEND, tmp_path / "backend", ignore=ExternalPortTest.IGNORED)
        source = variant / "testportbackend" / "port_backend.py"
        text = source.read_text(encoding="utf-8")
        assert text.count("Contract.BACKEND,") == 1
        source.write_text(text.replace("Contract.BACKEND,", "Contract.BACKEND + 1,"), encoding="utf-8")
        environment = ExtensionEnvironment(tmp_path / "environment")
        environment.create((), (variant,))

        failure = ExtensionEnvironment.error(environment.haylen("doctor", "--target", ExternalPortTest.TARGET, "--json"))
        assert failure["code"] == "incompatibleExtension"
        assert 'The backend "test-port" implements the backend contract 2, and this tooling implements 1. Install the version of the backend for contract 1.' in failure["message"]

    def test_a_plugin_without_an_implementation_for_the_target_stops_the_resolution(self, installed: ExtensionEnvironment, tmp_path: Path):
        app = shutil.copytree(ExternalPortTest.APP, tmp_path / "app")
        manifest = app / "plugins" / ExternalPortTest.PLUGIN / "plugin.json"
        document = json.loads(manifest.read_text(encoding="utf-8"))
        document["implementations"] = {"web": "lua"}
        document["contributions"] = []
        manifest.write_text(json.dumps(document), encoding="utf-8")

        failure = ExtensionEnvironment.error(installed.haylen("app", "build", app, "--target", ExternalPortTest.TARGET, "--json"))
        assert failure["code"] == "unsupportedTarget"
        assert 'The plugin "test-port-greeter" does not support "test-port". It supports "web", so remove it from the app to build for "test-port".' in failure["message"]

    def test_a_capability_that_the_backend_does_not_declare_stops_the_command(self, installed: ExtensionEnvironment, sdk: dict):
        failure = ExtensionEnvironment.error(self.app(installed, "package", "--format", "installer", "--config", "Debug", "--json"))
        assert failure == {"code": "missingCapability", "message": 'The backend "test-port" of "test-port" does not package apps.'}

    def test_an_architecture_that_the_target_does_not_build_stops_the_resolution(self, installed: ExtensionEnvironment):
        other = "x86_64" if Host.architecture() == "arm64" else "arm64"
        failure = ExtensionEnvironment.error(self.app(installed, "build", "--architecture", other, "--json"))
        assert failure["code"] == "unsupportedTarget"
        assert failure["message"] == f'The target "test-port" builds for "{Host.architecture()}", not for "{other}".'

    def test_an_sdk_whose_slice_lacks_its_files_stops_the_preparation(self, installed: ExtensionEnvironment, tmp_path: Path):
        architecture = Host.architecture()
        incomplete = {
            "format": 1,
            "port": "test-port",
            "portVersion": "1.0.0",
            "engineVersion": Repository.engine_version(),
            "revision": Repository.engine_revision(),
            "configuration": "Release",
            "slices": [
                {
                    "system": "test-port",
                    "environment": "host",
                    "architectures": [architecture],
                    "abi": f"test-port-{architecture}",
                    "toolchain": "test-port-cc",
                    "runtime": "host-c++",
                    "minimumSystemVersion": "1.0",
                    "graphicsApis": ["port"],
                    "capabilities": ["dynamicLibraries"],
                    "files": {"cmake": f"test-port-{architecture}/lib/cmake/haylen"},
                }
            ],
        }
        (tmp_path / "haylen-sdk.json").write_text(json.dumps(incomplete), encoding="utf-8")

        command = ("app", "build", ExternalPortTest.APP, "--target", ExternalPortTest.TARGET, "--engine-config", "Release", "--json")
        failure = ExtensionEnvironment.error(installed.haylen(*command, environment={"HAYLEN_SDK_PATH": str(tmp_path)}))
        assert failure["code"] == "incompatibleSdk"
        assert "is incomplete, since its slice names the CMake package" in failure["message"]
        assert 'Build it again with "haylen sdk build --target test-port --config Release".' in failure["message"]
