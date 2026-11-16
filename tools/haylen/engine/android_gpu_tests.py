import io
import shlex
import shutil
import subprocess
import tarfile
import threading
import time
import zipfile
from collections.abc import Mapping
from pathlib import Path
from typing import TextIO

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.android_test_device import AndroidTestDevice
from haylen.engine.desktop_gpu_tests import DesktopGpuTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.gpu_test_results import GpuTestResults
from haylen.toolchains.android_debug_bridge import AndroidDebugBridge
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.java import Java


class AndroidGpuTests:
    """Builds the GPU tests for the ABI of an Android device as the library of a NativeActivity, since a program that adb starts reaches the Vulkan driver of some devices only from an app. An APK carries the library with the reference images as assets, runs the tests once it starts, writes their output to the log and their results into its files, and quits with their status. The run installs it, prints what the tests print, copies their results and the images of the scenes that differ into the tree and uninstalls it."""

    APP = "com.haylen.gputests"
    LOG_TAG = "haylen-gpu-tests"
    MANIFEST = Repository.ENGINE / "tests" / "gpu" / "android" / "AndroidManifest.xml"
    REFERENCES = Repository.ENGINE / "tests" / "data" / "gpu"
    LIBRARY = "libhaylen-gpu-tests.so"
    # The key only signs the test app of a build tree, so its password keeps nothing secret.
    KEY_PASSWORD = "haylen-gpu-tests"
    KEY_SIZE = 2048
    KEY_DAYS = 10000
    LOG_BEGINNING = "--------- beginning of"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context
        self.sdk = AndroidSdk(context.environment)
        self.bridge = AndroidDebugBridge(context, self.sdk)

    def configure_command(self, tree: EngineTree, abi: str, configuration: str) -> list[object]:
        """The configure command of the tree of an ABI, which builds the library of the test app alone."""
        return tree.command(configuration, "-G", "Ninja", *AndroidNdk(self.sdk).cmake_options(abi), *EngineTree.ONLY_TESTS)

    def run(self, configuration: str, api: str | None, requested: str | None) -> dict[str, object]:
        device = AndroidTestDevice(self.context, self.sdk, requested)
        abi = device.abi()
        tree = EngineTree(self.context, f"android-gpu-tests-{abi}-{configuration.lower()}")
        tree.ensure(self.configure_command(tree, abi, configuration))
        tree.build([DesktopGpuTests.TARGET], configuration)
        apk = self.package(tree.folder, abi)

        output = tree.folder / "gpu-test-output"
        Folders.remove(output)
        with device as serial:
            self.context.terminal.step(f'Installing the GPU test app on "{serial}"')
            self.context.process.run(self.bridge.command(serial, "install", "-r", apk))
            self.print_renderer(serial)
            try:
                where = f'on "{api}" on "{serial}"' if api else f'on every device of the build on "{serial}"'
                self.context.terminal.step(f"Running the GPU tests of Android {where}")
                self.context.process.run(self.bridge.command(serial, "logcat", "-b", "crash", "-c"))
                self.context.process.run(self.launch_command(serial, api, self.context.environment))
                self.follow(serial)
                self.print_crash(serial)
            finally:
                self.copy_results(serial, output)
                self.context.process.run(self.bridge.command(serial, "uninstall", AndroidGpuTests.APP))

        problem = GpuTestResults.problem(output / "results.xml")
        GpuTestResults.show_images(output, self.context.terminal)
        if problem:
            raise BuildFailed(problem)
        self.context.terminal.success("The GPU tests of Android passed.")
        return {"tree": str(tree.folder), "abi": abi, "graphicsApi": api, "output": str(output)}

    def package(self, folder: Path, abi: str) -> Path:
        """Packages the library of the tests without its debug information, which the build strips, and the reference images into the APK of the test app, aligned and signed with a key that the tree keeps."""
        apk = folder / "apk"
        assets = apk / "assets"
        Folders.remove(assets)
        shutil.copytree(AndroidGpuTests.REFERENCES, assets / "gpu")
        jar, level = self.sdk.platform_jar()
        unsigned = apk / "unsigned.apk"
        link = [self.sdk.build_tool("aapt2"), "link", "--manifest", AndroidGpuTests.MANIFEST, "-I", jar, "--min-sdk-version", str(AndroidNdk.MINIMUM_API), "--target-sdk-version", str(level)]
        self.context.process.run([*link, "-A", assets, "-o", unsigned])
        with zipfile.ZipFile(unsigned, "a", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.write(folder / "bin" / "stripped" / AndroidGpuTests.LIBRARY, f"lib/{abi}/{AndroidGpuTests.LIBRARY}")
        aligned = apk / "aligned.apk"
        self.context.process.run([self.sdk.build_tool("zipalign"), "-f", "-p", "4", unsigned, aligned])

        # The password reaches the tools through the environment, as for the keys of apps.
        key = apk / "debug.jks"
        environment = {**self.context.environment, "HAYLEN_KEY_PASSWORD": AndroidGpuTests.KEY_PASSWORD}
        if not key.is_file():
            keytool = Java(self.context.environment).tool("keytool")
            generate = [keytool, "-genkeypair", "-keystore", key, "-storetype", "PKCS12", "-alias", "haylen-gpu-tests", "-keyalg", "RSA"]
            self.context.process.run(
                [*generate, "-keysize", str(AndroidGpuTests.KEY_SIZE), "-validity", str(AndroidGpuTests.KEY_DAYS), "-dname", "CN=Haylen GPU Tests", "-storepass:env", "HAYLEN_KEY_PASSWORD"],
                env=environment,
            )
        signed = apk / "haylen-gpu-tests.apk"
        self.context.process.run([self.sdk.build_tool("apksigner"), "sign", "--ks", key, "--ks-pass", "env:HAYLEN_KEY_PASSWORD", "--out", signed, aligned], env=environment)
        return signed

    def launch_command(self, serial: str, api: str | None, environment: Mapping[str, str]) -> list[object]:
        """The command that stops the test app if it runs, starts it again and waits for its activity, with the variables of the tests as the string array extra `environment`, whose items `am` splits at the commas they do not escape."""
        variables = {name: value for name, value in environment.items() if name.startswith(GpuTestResults.VARIABLE_PREFIXES)}
        if api:
            variables[DesktopGpuTests.API_VARIABLE] = api
        items = ",".join(f"{name}={value}".replace(",", "\\,") for name, value in sorted(variables.items()))
        extra = ["--esa", "environment", shlex.quote(items)] if items else []
        return self.bridge.command(serial, "shell", "am", "start", "-S", "-W", "-n", f"{AndroidGpuTests.APP}/android.app.NativeActivity", *extra)

    def process_id(self, serial: str) -> str:
        """The id of the process of the test app on the device, or an empty text when it does not run."""
        return self.context.process.capture(self.bridge.command(serial, "shell", f"pidof {AndroidGpuTests.APP} || true")).strip()

    def follow(self, serial: str) -> None:
        """Prints the output of the test app from the log of the device while it runs and returns once its process ends, with the lines that the log took after the last one it printed."""
        process = self.process_id(serial)
        if not process:
            return
        log = [str(part) for part in self.bridge.command(serial, "logcat", "-v", "raw", f"--pid={process}", "-s", f"{AndroidGpuTests.LOG_TAG}:I")]
        printed: list[str] = []
        with subprocess.Popen(log, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8", errors="replace") as reader:
            follower = threading.Thread(target=self.forward, args=(reader.stdout, printed), daemon=True)
            follower.start()
            while self.process_id(serial) == process:
                time.sleep(1)
            reader.terminate()
            follower.join()
        dumped = self.context.process.capture([*log[:4], "-d", *log[4:]]).splitlines()
        lines = [line for line in dumped if not line.startswith(AndroidGpuTests.LOG_BEGINNING)]
        for line in lines[len(printed) :]:
            self.context.terminal.verbatim(line)

    def forward(self, lines: TextIO, printed: list[str]) -> None:
        for line in lines:
            if not line.startswith(AndroidGpuTests.LOG_BEGINNING):
                printed.append(line.rstrip("\n"))
                self.context.terminal.verbatim(printed[-1])

    def print_renderer(self, serial: str) -> None:
        """Prints the OpenGL ES renderer that the system of the device reports, so the results of a run name the driver they come from."""
        surfaces = self.context.process.capture(self.bridge.command(serial, "shell", "dumpsys", "SurfaceFlinger")).splitlines()
        renderer = next((line.strip() for line in surfaces if line.strip().startswith("GLES:")), None)
        if renderer:
            self.context.terminal.info(f'The device draws OpenGL ES through "{renderer.removeprefix("GLES:").strip()}".')

    def print_crash(self, serial: str) -> None:
        """Prints the crash log of the device when the test app crashed during the run, which names the signal and the native frames of the crash."""
        crash = self.context.process.capture(self.bridge.command(serial, "logcat", "-d", "-b", "crash")).splitlines()
        lines = [line for line in crash if not line.startswith(AndroidGpuTests.LOG_BEGINNING)]
        if not lines:
            return
        self.context.terminal.warning("The GPU test app crashed. The crash log of the device follows.")
        self.context.terminal.verbatim("\n".join(lines[-80:]))

    def copy_results(self, serial: str, output: Path) -> None:
        """Copies the results of the test app and the images of the scenes that differ into the tree through `run-as`, since the files of an app are its own. A run that wrote nothing, such as one that crashed first, leaves nothing to copy."""
        test = [str(part) for part in self.bridge.command(serial, "shell", "run-as", AndroidGpuTests.APP, "test", "-d", f"files/{output.name}")]
        if subprocess.run(test, capture_output=True, check=False).returncode != 0:
            return
        # The command "exec-out" ends with the status 0 even when the command on the device fails, and hands its error text as the output.
        archive = subprocess.run(
            [str(part) for part in self.bridge.command(serial, "exec-out", "run-as", AndroidGpuTests.APP, "tar", "-c", "-C", "files", output.name)], capture_output=True, check=False
        )
        if archive.returncode != 0 or not archive.stdout:
            return
        with tarfile.open(fileobj=io.BytesIO(archive.stdout)) as files:
            files.extractall(output.parent, filter="data")
