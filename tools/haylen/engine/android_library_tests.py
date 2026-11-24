from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.android_test_device import AndroidTestDevice
from haylen.engine.engine_tree import EngineTree
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.gradle import Gradle


class AndroidLibraryTests:
    """Runs the tests of the Android libraries of the engine with Gradle: the tests of `src/test` on the Java of this machine and the instrumented tests of `src/androidTest` on an Android device, which Gradle installs in a test app, runs and uninstalls. The test app carries the player library of the ABI of the device, which a tree of its own builds, since the instrumented tests reach its native methods."""

    TASKS = (":haylen:testDebugUnitTest", ":haylen:connectedDebugAndroidTest")

    def __init__(self, context: ToolingContext) -> None:
        self.context = context
        self.sdk = AndroidSdk(context.environment)

    @staticmethod
    def players(tree: EngineTree) -> Path:
        """The folder of the player library of the tree, in a subfolder named after its ABI, as the Android libraries take it."""
        return tree.folder / "players"

    def configure_command(self, tree: EngineTree, abi: str, configuration: str) -> list[object]:
        """The configure command of the tree of an ABI, which builds the player library alone into the subfolder of its ABI."""
        options = ["-DHAYLEN_BUILD_PLAYER=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_SDK=OFF", "-DHAYLEN_BUILD_TOOLS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF"]
        output = f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={(AndroidLibraryTests.players(tree) / abi).as_posix()}"
        return tree.command(configuration, "-G", "Ninja", *AndroidNdk(self.sdk).cmake_options(abi), *options, output)

    def run(self, configuration: str, requested: str | None) -> dict[str, object]:
        device = AndroidTestDevice(self.context, self.sdk, requested)
        abi = device.abi()
        tree = EngineTree(self.context, f"android-library-tests-{abi}-{configuration.lower()}")
        tree.ensure(self.configure_command(tree, abi, configuration))
        tree.build(["haylen"], configuration)

        players = AndroidLibraryTests.players(tree)
        varn = tree.tree.cache_value("varn_SOURCE_DIR")
        if varn is None:
            raise BuildFailed(f"The engine tree `{Repository.shown(tree.folder)}` did not finish its configure. Run the command again.")

        outputs = tree.folder / "gradle"
        with device as serial:
            self.context.terminal.step(f'Running the tests of the Android libraries on this machine and on "{serial}"')
            command: list[object] = [Gradle.path(self.context), "-p", AndroidPlatform.LIBRARY_PROJECT, *AndroidLibraryTests.TASKS, f"--max-workers={self.context.jobs}"]
            command += ["--project-cache-dir", outputs / "cache", f"-PhaylenNativeLibraries={players}", f"-PhaylenVarnSourceDir={varn}"]
            command += [f"-PhaylenMavenDir={outputs / 'maven'}", f"-PhaylenBuildDirectory={outputs}"]
            self.context.process.run(command, env={**self.context.environment, "ANDROID_SERIAL": serial})
        self.context.terminal.success("The tests of the Android libraries passed.")
        return {"tree": str(tree.folder), "abi": abi, "reports": str(outputs / "haylen" / "reports")}
