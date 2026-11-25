import contextlib
import json
import math
import time
from collections.abc import Iterator

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.apple.ios_devices import IosDevices


class AppleSimulators:
    """The simulators of Xcode on this Mac, by the family of their runtime (`iOS`, `tvOS` or `xrOS`). The tooling never creates or deletes one, and it shuts down every simulator that it boots."""

    # The platform of each family that "xcodebuild -downloadPlatform" installs the runtime of.
    DOWNLOAD_PLATFORMS = {"iOS": "iOS", "tvOS": "tvOS", "xrOS": "visionOS"}
    # SpringBoard of iOS writes this report to the unified log once its notification center loaded the apps whose notifications it shows. An app that installs before the report never becomes one of them, so its requests for notifications hang or fail, and a busy Mac delays the report for minutes after the boot.
    NOTIFICATION_SOURCES = 'process == "SpringBoard" AND subsystem == "com.apple.UserNotifications" AND eventMessage CONTAINS "notificationSourcesDidInstall" AND eventMessage CONTAINS "initialLoad=1"'
    NOTIFICATION_SECONDS = 900
    POLL_SECONDS = 5

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def available(self, family: str) -> list[dict]:
        devices = json.loads(self.context.process.capture(["xcrun", "simctl", "list", "devices", "available", "--json"]))["devices"]
        return [device for runtime, entries in devices.items() if f"SimRuntime.{family}-" in runtime for device in entries]

    @staticmethod
    def device_kind(device: dict) -> str:
        """Whether an iOS simulator is an iPhone or an iPad, by the names of "platforms.ios.devices"."""
        return IosDevices.IPAD if ".iPad" in device["deviceTypeIdentifier"] else IosDevices.IPHONE

    def find(self, family: str, requested: str | None, devices: tuple[str, ...] = IosDevices.DEFAULT) -> dict:
        """The simulator that `--device` names by its name or identifier, or else a booted one of the family, or else the first of the family. On iOS it takes a simulator of the devices of the app, an iPhone first, and an app for iPhones alone takes an iPad only when no iPhone is installed, since iPadOS runs iPhone apps."""
        candidates = self.available(family)
        ipads_only = devices == (IosDevices.IPAD,)
        if requested:
            matches = [device for device in candidates if requested in (device["udid"], device["name"])]
            if not matches:
                raise UnsupportedTarget(f'No available {family} simulator has the name or identifier "{requested}". {UnsupportedTarget.DEVICES_HINT}')
            if family == "iOS" and ipads_only and AppleSimulators.device_kind(matches[0]) != IosDevices.IPAD:
                raise UnsupportedTarget(
                    f'The app runs on iPads alone, as "platforms.ios.devices" of its `haylen.json` says, so it does not install on the iPhone simulator "{matches[0]["name"]}". Name an iPad simulator with "--device". {UnsupportedTarget.DEVICES_HINT}'
                )
            return matches[0]
        if not candidates:
            raise UnsupportedTarget(f'No {family} simulator is installed. Install the runtime with "xcodebuild -downloadPlatform {AppleSimulators.DOWNLOAD_PLATFORMS[family]}".')

        booted = [device for device in candidates if device["state"] == "Booted"]
        if family != "iOS":
            return (booted or candidates)[0]
        fitting = [device for device in candidates if AppleSimulators.device_kind(device) in devices]
        if not fitting and ipads_only:
            raise UnsupportedTarget(
                'The app runs on iPads alone, as "platforms.ios.devices" of its `haylen.json` says, and no iPad simulator is installed. Add one in the Devices and Simulators window of Xcode.'
            )
        first = [device for device in fitting if AppleSimulators.device_kind(device) == devices[0]]
        return ([device for device in booted if device in fitting] or first or candidates)[0]

    @contextlib.contextmanager
    def booted(self, family: str, requested: str | None, devices: tuple[str, ...] = IosDevices.DEFAULT) -> Iterator[dict]:
        """Yields a simulator of the devices of the app that runs: one that was booted already, or one that it boots without a window and shuts down afterwards. An iOS simulator that it boots is ready once its notification center loaded its apps."""
        simulator = self.find(family, requested, devices)
        booted = simulator["state"] == "Booted"
        started = time.monotonic()
        if not booted:
            self.context.terminal.step(f'Booting the simulator "{simulator["name"]}" without a window')
            # The flag of `bootstatus` that boots a simulator fails on some devices, such as the foldable iPhone, so the simulator boots on its own and `bootstatus` only waits for it.
            self.context.process.run(["xcrun", "simctl", "boot", simulator["udid"]])
        try:
            if not booted:
                self.context.process.run(["xcrun", "simctl", "bootstatus", simulator["udid"]])
                if family == "iOS":
                    self.wait_for_notification_center(simulator, started)
            yield simulator
        finally:
            if not booted:
                self.context.process.run(["xcrun", "simctl", "shutdown", simulator["udid"]])

    @staticmethod
    def log_command(udid: str, seconds: float) -> list[str]:
        """The command that finds the report of the notification center in the unified log of a simulator over the last whole minutes that cover that many seconds."""
        minutes = math.ceil(seconds / 60) + 1
        return ["xcrun", "simctl", "spawn", udid, "log", "show", "--last", f"{minutes}m", "--style", "compact", "--predicate", AppleSimulators.NOTIFICATION_SOURCES]

    def wait_for_notification_center(self, simulator: dict, started: float) -> None:
        """Waits until the notification center of an iOS simulator that just booted loaded its apps, so the apps that install afterwards become apps that it shows notifications of."""
        self.context.terminal.step(f'Waiting for the notification center of "{simulator["name"]}"')
        deadline = started + AppleSimulators.NOTIFICATION_SECONDS
        while "initialLoad=1" not in self.context.process.capture(AppleSimulators.log_command(simulator["udid"], time.monotonic() - started)):
            if time.monotonic() > deadline:
                raise BuildFailed(
                    f'The notification center of the simulator "{simulator["name"]}" did not load its apps within {AppleSimulators.NOTIFICATION_SECONDS // 60} minutes of the boot, so an app that installs now would never show a notification. Restart the simulator and run the command again.'
                )
            time.sleep(AppleSimulators.POLL_SECONDS)
