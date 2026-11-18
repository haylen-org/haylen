import subprocess
from collections.abc import Mapping

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.tooling_context import ToolingContext
from haylen.engine.desktop_gpu_tests import DesktopGpuTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.gpu_test_results import GpuTestResults
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.apple_simulators import AppleSimulators


class AppleDeviceGpuTests:
    """Builds the GPU tests of an iOS, tvOS or visionOS device as an app that Xcode signs for the development team that `HAYLEN_APPLE_TEAM` names, installs and launches it with devicectl and copies back its results and the images of the scenes that differ, which the app keeps in the caches of its container, before it uninstalls the app."""

    SLICES = {"ios": "ios", "tvos": "tvos", "visionos": "xros"}
    APP = "com.haylen.gputests"
    DEVICE_OUTPUT = "Library/Caches/gpu-test-output"
    TEAM_VARIABLE = "HAYLEN_APPLE_TEAM"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def team(self) -> str:
        team = self.context.environment.get(AppleDeviceGpuTests.TEAM_VARIABLE)
        if not team:
            raise MissingToolchain(f'The GPU tests of Apple devices are signed with a development team. Set "{AppleDeviceGpuTests.TEAM_VARIABLE}" to its identifier.')
        return team

    def configure_command(self, tree: EngineTree, platform: str, configuration: str) -> list[object]:
        options = AppleSdk.cmake_options(AppleDeviceGpuTests.SLICES[platform], "arm64")
        return tree.command(configuration, "-G", "Xcode", *options, f"-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM={self.team()}", *EngineTree.ONLY_TESTS)

    @staticmethod
    def command(device: str, api: str | None, environment: Mapping[str, str]) -> tuple[list[object], dict[str, str]]:
        """The command and the environment that launch the test app on a device and wait for it to quit, which takes the variables of the tests through the prefix of devicectl."""
        env = dict(environment)
        if api:
            env[DesktopGpuTests.API_VARIABLE] = api
        env.update({f"DEVICECTL_CHILD_{name}": value for name, value in list(env.items()) if name.startswith(GpuTestResults.VARIABLE_PREFIXES)})
        return ["xcrun", "devicectl", "device", "process", "launch", "--console", "--terminate-existing", "--device", device, AppleDeviceGpuTests.APP], env

    def run(self, platform: str, configuration: str, api: str | None, device: str | None) -> dict[str, object]:
        if Host.system() != "macos":
            raise UnsupportedTarget(f'The GPU tests of "{platform}" build only on macOS, and this machine runs "{Host.system()}".')
        if not device:
            raise UnsupportedTarget(f'Name the device with "--device". {AppleSimulators.DEVICES_HINT}')
        tree = EngineTree(self.context, f"{platform}-gpu-tests-{configuration.lower()}")
        tree.ensure(self.configure_command(tree, platform, configuration))
        tree.build([DesktopGpuTests.TARGET], configuration, "-allowProvisioningUpdates")

        output = tree.folder / "gpu-test-output"
        Folders.remove(output)
        devicectl = ["xcrun", "devicectl", "device"]
        self.context.terminal.step(f'Installing the GPU test app on "{device}"')
        self.context.process.run([*devicectl, "install", "app", "--device", device, tree.folder / "bin" / "haylen-gpu-tests.app"])
        command, env = AppleDeviceGpuTests.command(device, api, self.context.environment)
        self.context.terminal.step(f'Running the GPU tests on "{api}" on "{device}"' if api else f'Running the GPU tests on every device of the build on "{device}"')
        failure: BuildFailed | None = None
        try:
            self.context.process.run(command, env=env, stream=True)
        except BuildFailed as error:
            failure = error
        finally:
            # The folder exists only once the tests wrote into it, so an app that crashed before has nothing to copy back.
            copy = [*devicectl, "copy", "from", "--device", device, "--domain-type", "appDataContainer", "--domain-identifier", AppleDeviceGpuTests.APP]
            subprocess.run([*copy, "--source", AppleDeviceGpuTests.DEVICE_OUTPUT, "--destination", str(output)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
            self.context.process.run([*devicectl, "uninstall", "app", "--device", device, AppleDeviceGpuTests.APP])

        # The command of devicectl exits with the status of the app, so the results say which tests failed, and only a launch that left no results stops with its own error.
        results = output / "results.xml"
        if failure is not None and not results.is_file():
            raise failure
        problem = GpuTestResults.problem(results)
        GpuTestResults.show_images(output, self.context.terminal)
        if problem:
            raise BuildFailed(problem)
        self.context.terminal.success(f'The GPU tests passed on "{device}".')
        return {"tree": str(tree.folder), "target": platform, "device": device, "graphicsApi": api, "output": str(output)}
