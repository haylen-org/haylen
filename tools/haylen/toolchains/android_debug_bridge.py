from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.android_sdk import AndroidSdk


class AndroidDebugBridge:
    """The `adb` of the Android SDK, through which the tooling finds the connected devices and emulators and reads what they run."""

    DEVICES_HINT = 'Use the command "haylen devices" to list the simulators, emulators and devices of this machine.'

    def __init__(self, context: ToolingContext, sdk: AndroidSdk) -> None:
        self.context = context
        self.sdk = sdk

    def command(self, serial: str, *arguments: object) -> list[object]:
        """The command line of `adb` for one device."""
        return [self.sdk.adb, "-s", serial, *arguments]

    def devices(self) -> list[str]:
        """The serials of the devices and emulators that are connected and ready."""
        lines = self.context.process.capture([self.sdk.adb, "devices"]).splitlines()[1:]
        return [line.split()[0] for line in lines if line.strip().endswith("device")]

    def device(self, requested: str | None) -> str:
        """The device that `--device` names, or the only one that is connected."""
        devices = self.devices()
        if requested:
            if requested not in devices:
                raise UnsupportedTarget(f'The device "{requested}" is not a connected Android device. {AndroidDebugBridge.DEVICES_HINT}')
            return requested
        if not devices:
            raise UnsupportedTarget(f'Name the Android device with "--device", because none is connected. {AndroidDebugBridge.DEVICES_HINT}')
        if len(devices) > 1:
            names = ", ".join(f'"{name}"' for name in devices)
            raise UnsupportedTarget(f'Name the Android device with "--device", because {len(devices)} are connected: {names}. {AndroidDebugBridge.DEVICES_HINT}')
        return devices[0]

    def property(self, serial: str, name: str) -> str:
        return self.context.process.capture(self.command(serial, "shell", "getprop", name)).strip()

    def abi(self, serial: str) -> str:
        """The ABI that a device runs, which must be one that the engine builds for."""
        abi = self.property(serial, "ro.product.cpu.abi")
        if abi not in AndroidNdk.ABIS:
            names = ", ".join(f'"{name}"' for name in AndroidNdk.ABIS)
            raise UnsupportedTarget(f'The device "{serial}" runs "{abi}", while the engine builds for {names}. Use a device or an emulator of one of those.')
        return abi

    def name(self, serial: str) -> str:
        """The name that people know a device by: the virtual device of an emulator, or the model of a phone, a tablet or a TV."""
        if serial.startswith("emulator-"):
            return self.context.process.capture(self.command(serial, "emu", "avd", "name")).splitlines()[0].strip()
        return self.property(serial, "ro.product.model")
