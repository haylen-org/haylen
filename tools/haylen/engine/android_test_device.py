import subprocess
import time
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.android_debug_bridge import AndroidDebugBridge
from haylen.toolchains.android_sdk import AndroidSdk


class AndroidTestDevice:
    """The Android device that the GPU tests run on: the one that `--device` names, the only one connected, or else an emulator of the first virtual device, which it starts without a window and shuts down afterwards. With `HAYLEN_TEST_GPU_SOFTWARE=1` that emulator draws through SwiftShader, as a machine without a GPU for it needs."""

    EMULATOR_PORT = 5554
    BOOT_SECONDS = 300
    # An emulator that the request to end leaves running this long is killed.
    EXIT_SECONDS = 60

    def __init__(self, context: ToolingContext, sdk: AndroidSdk, requested: str | None) -> None:
        self.context = context
        self.sdk = sdk
        self.bridge = AndroidDebugBridge(context, sdk)
        self.requested = requested
        self.emulator: subprocess.Popen | None = None
        self.serial = ""

    def virtual_devices(self) -> list[str]:
        return self.context.process.capture([self.sdk.emulator, "-list-avds"]).split()

    def virtual_device_abi(self, name: str) -> str:
        """The ABI of a virtual device, from the configuration that Android Studio writes for it."""
        folder = Path(self.context.environment.get("ANDROID_AVD_HOME", Path.home() / ".android" / "avd"))
        for line in (folder / f"{name}.avd" / "config.ini").read_text(encoding="utf-8").splitlines():
            if line.startswith("abi.type="):
                return line.split("=", 1)[1].strip()
        raise UnsupportedTarget(f'The virtual device "{name}" names no ABI in its configuration. Create it again with the Device Manager of Android Studio.')

    def first_virtual_device(self) -> str:
        devices = self.virtual_devices()
        if not devices:
            raise UnsupportedTarget(
                "No Android device is connected and the Android SDK has no virtual device to start. Connect a device, or create a virtual device with the Device Manager of Android Studio."
            )
        return devices[0]

    def uses_connected(self) -> bool:
        return bool(self.requested) or bool(self.bridge.devices())

    def abi(self) -> str:
        """The ABI that the tests build for, which is the one of the device they will run on."""
        if self.uses_connected():
            return self.bridge.abi(self.bridge.device(self.requested))
        return self.virtual_device_abi(self.first_virtual_device())

    def __enter__(self) -> str:
        if self.uses_connected():
            self.serial = self.bridge.device(self.requested)
            return self.serial

        name = self.first_virtual_device()
        self.serial = f"emulator-{AndroidTestDevice.EMULATOR_PORT}"
        self.context.terminal.step(f'Starting the emulator of "{name}" without a window')
        software = ["-gpu", "swiftshader_indirect"] if self.context.environment.get("HAYLEN_TEST_GPU_SOFTWARE") == "1" else []
        command = [self.sdk.emulator, "-avd", name, "-port", str(AndroidTestDevice.EMULATOR_PORT), "-no-window", "-no-audio", "-no-boot-anim", "-no-snapshot-save", *software]
        self.emulator = subprocess.Popen([str(part) for part in command], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            self.wait_for_boot(name)
        except BaseException:
            self.__exit__()
            raise
        return self.serial

    def wait_for_boot(self, name: str) -> None:
        deadline = time.monotonic() + AndroidTestDevice.BOOT_SECONDS
        while self.serial not in self.bridge.devices() or self.bridge.property(self.serial, "sys.boot_completed") != "1":
            if self.emulator.poll() is not None or time.monotonic() > deadline:
                raise BuildFailed(f'The emulator of "{name}" did not start within {AndroidTestDevice.BOOT_SECONDS} seconds.')
            time.sleep(2)

    def __exit__(self, *_: object) -> None:
        if self.emulator is None:
            return
        subprocess.run([str(self.sdk.adb), "-s", self.serial, "emu", "kill"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        try:
            self.emulator.wait(timeout=AndroidTestDevice.EXIT_SECONDS)
        except subprocess.TimeoutExpired:
            self.emulator.kill()
            self.emulator.wait()
        self.emulator = None
