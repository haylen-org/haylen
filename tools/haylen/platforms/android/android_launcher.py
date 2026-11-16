import shlex
import subprocess
import threading

from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext
from haylen.development.app_run import AppRun
from haylen.development.development_reach import DevelopmentReach
from haylen.platforms.android.android_bundle_install import AndroidBundleInstall
from haylen.platforms.android.android_gradle_build import AndroidGradleBuild
from haylen.platforms.android.android_native_build import AndroidNativeBuild
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.device import Device
from haylen.platforms.launch_request import LaunchRequest
from haylen.toolchains.android_debug_bridge import AndroidDebugBridge
from haylen.toolchains.android_sdk import AndroidSdk


class AndroidLauncher:
    """Lists the Android devices and emulators that adb reaches, and runs a built app on one of them: it installs the APK, or the bundle of a release whose asset packs Google Play delivers after the install, starts the app from its launcher intent and streams the log of its process, filtered to the lines of the engine, of the plugins of the app and the crashes of its Java code, until the app ends or the developer quits. Android starts apps without a command line, so the address of the development server travels as an extra of the intent, which only a debuggable app reads, and the port of the server reaches the device through `adb reverse`, which the run removes when it ends. The app stays installed, as the developer expects of a run."""

    ACTIVITY = "dev.haylen.HaylenActivity"
    DEVELOPMENT_EXTRA = "dev.haylen.developmentServer"
    # The log shows the lines of the engine as the engine writes them, the lines of the plugins, which log under their id, and the crashes of the Java code with their level and tag.
    LOG_TAG = "haylen"
    CRASH_TAG = "AndroidRuntime"
    POLL_SECONDS = 1.0

    def __init__(self, context: ToolingContext) -> None:
        self.context = context
        self.sdk = AndroidSdk(context.environment)
        self.bridge = AndroidDebugBridge(context, self.sdk)

    def devices(self) -> tuple[Device, ...]:
        """The devices and emulators that adb reaches, with their model or virtual device and their version of Android, and the ones it cannot use yet with their state."""
        found = []
        for line in self.context.process.capture([self.sdk.adb, "devices"]).splitlines()[1:]:
            if not line.strip():
                continue
            serial, state = line.split()[:2]
            kind = "emulator" if serial.startswith("emulator-") else "device"
            if state != "device":
                found.append(Device(serial, serial, kind, state, (AndroidPlatform.TARGET,)))
                continue
            version = self.bridge.property(serial, "ro.build.version.release")
            found.append(Device(serial, f"{self.bridge.name(serial)}, Android {version}", kind, "ready", (AndroidPlatform.TARGET,)))
        return tuple(found)

    @staticmethod
    def log_line(line: str) -> str | None:
        """Turns a line of logcat in its "tag" format into the line that the terminal shows: a line of the engine as the engine wrote it, a line of another tag with its level and tag in front, and nothing for the lines where logcat names the buffer it starts."""
        if line.startswith("--------- beginning of"):
            return None
        prefix, separator, message = line.partition(": ")
        if separator and prefix[1:2] == "/" and prefix[2:].strip() == AndroidLauncher.LOG_TAG:
            return message
        return line

    def pid(self, serial: str, package: str) -> str:
        """The process of an app on a device, or an empty text when it runs none."""
        completed = subprocess.run([str(part) for part in self.bridge.command(serial, "shell", "pidof", package)], capture_output=True, text=True, encoding="utf-8", errors="replace", check=False)
        return completed.stdout.strip()

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        resolved = product.prepared.product
        serial = self.bridge.device(launch.device)
        abi = self.bridge.abi(serial)
        abis = [project.target.abi for project in resolved.projects]
        if abi not in abis:
            built = ", ".join(f'"{name}"' for name in abis)
            raise UnsupportedTarget(f'The device "{serial}" runs "{abi}", and the app was built for {built}. Build it for the device with "--architecture {abi}", or with every ABI of the target.')
        name = self.bridge.name(serial)
        package = resolved.app.identifier
        shipped = product.prepared.project / AndroidNativeBuild.PACKAGE

        with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, shipped) as run:
            self.install(product, serial, name)
            forward = f"tcp:{run.port}" if run.port is not None else None
            if forward is not None:
                self.context.process.run(self.bridge.command(serial, "reverse", forward, forward))
            try:
                self.start(serial, package, run.address)
                pid = self.pid(serial, package)
                if not pid:
                    raise BuildFailed(f'The app "{package}" stopped as soon as it started. Its log stays on the device, which "adb -s {serial} logcat" shows.')
                run.show(f'{name} ("{serial}")', lambda: self.stop(run, serial, package))
                threading.Thread(target=self.watch, args=(run, serial, package, pid), daemon=True).start()
                filters = [f"{AndroidLauncher.LOG_TAG}:V", *(f"{plugin.id}:V" for plugin in resolved.plugins), f"{AndroidLauncher.CRASH_TAG}:E", "*:S"]
                run.follow(self.bridge.command(serial, "logcat", "-v", "tag", f"--pid={pid}", *filters), line=AndroidLauncher.log_line)
            finally:
                if forward is not None:
                    subprocess.run([str(part) for part in self.bridge.command(serial, "reverse", "--remove", forward)], capture_output=True, check=False)

    def install(self, product: BuildProduct, serial: str, name: str) -> None:
        resolved = product.prepared.product
        if resolved.configuration is Configuration.RELEASE and AndroidGradleBuild.names(product.prepared, AndroidGradleBuild.DELIVERED_PACKS):
            bundle = AndroidGradleBuild.bundle(product.prepared)
            AndroidBundleInstall(self.context).install(bundle, resolved.app.folder, serial, bundle.with_suffix(".apks"))
            return
        self.context.terminal.step(f'Installing the app on "{name}"')
        self.context.process.run(self.bridge.command(serial, "install", "-r", product.path))

    def start(self, serial: str, package: str, address: str | None) -> None:
        """Starts the task of the app with the launcher intent, so the launcher icon brings that task back as it is later. The shell of the device reads the command line, so the extra is quoted for it."""
        extras = ["--es", AndroidLauncher.DEVELOPMENT_EXTRA, shlex.quote(address)] if address is not None else []
        intent = ["-a", "android.intent.action.MAIN", "-c", "android.intent.category.LAUNCHER", "-n", f"{package}/{AndroidLauncher.ACTIVITY}"]
        self.context.process.run(self.bridge.command(serial, "shell", "am", "start", "-W", *intent, *extras))

    def stop(self, run: AppRun, serial: str, package: str) -> None:
        """Ends the app, which may have ended already, and the log that follows it."""
        subprocess.run([str(part) for part in self.bridge.command(serial, "shell", "am", "force-stop", package)], capture_output=True, check=False)
        if run.process is not None:
            run.process.terminate()

    def watch(self, run: AppRun, serial: str, package: str, pid: str) -> None:
        """Ends the run once the process of the app is gone, such as when the app quits or crashes, since its log never ends on its own."""
        while not run.quitting.wait(AndroidLauncher.POLL_SECONDS):
            if self.pid(serial, package) != pid:
                self.context.terminal.info("The app stopped.")
                run.quit()
                return
