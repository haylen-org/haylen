import contextlib
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.desktop_gpu_tests import DesktopGpuTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.gpu_test_results import GpuTestResults
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.apple_simulators import AppleSimulators


class AppleGpuTests:
    """Builds the GPU tests for an Apple simulator or Mac Catalyst in a tree of their own for the architecture of this Mac and runs them, inside the simulator through simctl or on this Mac. They read the reference images and keep the images of the scenes that differ at their paths on this Mac, whose files the simulators share."""

    SLICES = {"ios-simulator": "ios-simulator", "tvos-simulator": "tvos-simulator", "visionos-simulator": "xros-simulator", "catalyst": AppleSdk.CATALYST}
    FAMILIES = {"ios-simulator": "iOS", "tvos-simulator": "tvOS", "visionos-simulator": "xrOS"}

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @staticmethod
    def configure_command(tree: EngineTree, platform: str, configuration: str) -> list[object]:
        architecture = "arm64" if Host.architecture() == "arm64" else "x86_64"
        return tree.command(configuration, "-G", "Ninja", *AppleSdk.cmake_options(AppleGpuTests.SLICES[platform], architecture), *EngineTree.ONLY_TESTS)

    @staticmethod
    def command(executable: Path, simulator: str | None, api: str | None, environment: Mapping[str, str]) -> tuple[list[object], dict[str, str]]:
        """The command and the environment that run the tests: the executable itself on Mac Catalyst, or the executable spawned inside the simulator of that identifier, which takes the variables of the tests through the prefix of simctl."""
        env = dict(environment)
        if api:
            env[DesktopGpuTests.API_VARIABLE] = api
        if simulator is None:
            return [executable], env
        env.update({f"SIMCTL_CHILD_{name}": value for name, value in list(env.items()) if name.startswith(GpuTestResults.VARIABLE_PREFIXES)})
        return ["xcrun", "simctl", "spawn", simulator, executable], env

    def run(self, platform: str, configuration: str, api: str | None, requested: str | None) -> dict[str, object]:
        if Host.system() != "macos":
            raise UnsupportedTarget(f'The GPU tests of "{platform}" build only on macOS, and this machine runs "{Host.system()}".')
        tree = EngineTree(self.context, f"{platform}-gpu-tests-{configuration.lower()}")
        tree.ensure(AppleGpuTests.configure_command(tree, platform, configuration))
        tree.build([DesktopGpuTests.TARGET], configuration)

        output = tree.folder / "gpu-test-output"
        Folders.remove(output)
        family = AppleGpuTests.FAMILIES.get(platform)
        with contextlib.ExitStack() as stack:
            simulator = stack.enter_context(AppleSimulators(self.context).booted(family, requested)) if family else None
            command, env = AppleGpuTests.command(tree.binary(DesktopGpuTests.TARGET), simulator["udid"] if simulator else None, api, self.context.environment)
            where = f'the simulator "{simulator["name"]}"' if simulator else "Mac Catalyst"
            self.context.terminal.step(f'Running the GPU tests on "{api}" in {where}' if api else f"Running the GPU tests on every device of the build in {where}")
            try:
                self.context.process.run(command, cwd=Repository.ENGINE / "tests", env=env, stream=True)
            finally:
                if output.is_dir():
                    self.context.terminal.info(f"The images of the scenes that differ are in `{Repository.shown(output)}`.")
        self.context.terminal.success(f"The GPU tests passed in {where}.")
        return {"tree": str(tree.folder), "platform": platform, "graphicsApi": api, "output": str(output)}
