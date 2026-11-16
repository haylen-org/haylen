import contextlib
import json
from collections.abc import Iterator

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext


class AppleSimulators:
    """The simulators of Xcode on this Mac, by the family of their runtime (`iOS`, `tvOS` or `xrOS`). The tooling never creates or deletes one, and it shuts down every simulator that it boots."""

    # The platform of each family that "xcodebuild -downloadPlatform" installs the runtime of.
    DOWNLOAD_PLATFORMS = {"iOS": "iOS", "tvOS": "tvOS", "xrOS": "visionOS"}
    DEVICES_HINT = 'Use the command "haylen devices" to list the simulators, emulators and devices of this machine.'

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def available(self, family: str) -> list[dict]:
        devices = json.loads(self.context.process.capture(["xcrun", "simctl", "list", "devices", "available", "--json"]))["devices"]
        return [device for runtime, entries in devices.items() if f"SimRuntime.{family}-" in runtime for device in entries]

    def find(self, family: str, requested: str | None) -> dict:
        """The simulator that `--device` names by its name or identifier, or else a booted one of the family, or else the first iPhone or the first of the family."""
        candidates = self.available(family)
        if requested:
            matches = [device for device in candidates if requested in (device["udid"], device["name"])]
            if not matches:
                raise UnsupportedTarget(f'No available {family} simulator has the name or identifier "{requested}". {AppleSimulators.DEVICES_HINT}')
            return matches[0]
        if not candidates:
            raise UnsupportedTarget(f'No {family} simulator is installed. Install the runtime with "xcodebuild -downloadPlatform {AppleSimulators.DOWNLOAD_PLATFORMS[family]}".')
        booted = [device for device in candidates if device["state"] == "Booted"]
        preferred = [device for device in candidates if device["name"].startswith("iPhone")] if family == "iOS" else []
        return (booted or preferred or candidates)[0]

    @contextlib.contextmanager
    def booted(self, family: str, requested: str | None) -> Iterator[dict]:
        """Yields a simulator that runs: one that was booted already, or one that it boots without a window and shuts down afterwards."""
        simulator = self.find(family, requested)
        booted = simulator["state"] == "Booted"
        if not booted:
            self.context.terminal.step(f'Booting the simulator "{simulator["name"]}" without a window')
            self.context.process.run(["xcrun", "simctl", "bootstatus", simulator["udid"], "-b"])
        try:
            yield simulator
        finally:
            if not booted:
                self.context.process.run(["xcrun", "simctl", "shutdown", simulator["udid"]])
