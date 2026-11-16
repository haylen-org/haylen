import json
import re
import tempfile
from pathlib import Path

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.device import Device
from haylen.toolchains.apple_simulators import AppleSimulators


class AppleDevices:
    """The simulators of iOS, tvOS and visionOS that Xcode has on this Mac and the iPhones, iPads, Apple TVs and Apple Vision Pros paired with it, by the identifier that `--device` takes."""

    # The simulator target of each family of simulator runtimes and the device target of each platform that devicectl reports.
    SIMULATOR_TARGETS = {"iOS": "ios-simulator", "tvOS": "tvos-simulator", "xrOS": "visionos-simulator"}
    DEVICE_TARGETS = {"iOS": "ios", "tvOS": "tvos", "visionOS": "visionos"}

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def all(self) -> tuple[Device, ...]:
        return (*self.simulators(), *self.physical())

    def simulators(self) -> list[Device]:
        listing = json.loads(self.context.process.capture(["xcrun", "simctl", "list", "devices", "available", "--json"]))["devices"]
        found = []
        for runtime, entries in listing.items():
            match = re.search(r"SimRuntime\.(\w+)-([\d-]+)$", runtime)
            if match is None or match[1] not in AppleDevices.SIMULATOR_TARGETS:
                continue
            target = AppleDevices.SIMULATOR_TARGETS[match[1]]
            found += [Device(entry["udid"], entry["name"], "simulator", entry["state"].lower(), (target,)) for entry in entries]
        return found

    def physical(self) -> list[Device]:
        """The devices paired with this Mac, as devicectl reports them in its JSON file."""
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "devices.json"
            self.context.process.capture(["xcrun", "devicectl", "list", "devices", "--quiet", "--omit-deprecated-fields-in-json", "--json-output", output])
            devices = json.loads(output.read_text(encoding="utf-8"))["result"]["devices"]
        found = []
        for device in devices:
            properties = device["properties"]
            hardware = properties["hardware"]
            if hardware.get("reality") != "physical" or hardware.get("platform") not in AppleDevices.DEVICE_TARGETS:
                continue
            state = properties.get("connection", {}).get("state", "unavailable")
            found.append(Device(hardware["udid"], properties["state"]["name"], "device", state, (AppleDevices.DEVICE_TARGETS[hardware["platform"]],)))
        return found

    def device(self, requested: str | None, target: str) -> Device:
        """The paired device of a target that `--device` names by its identifier or its name."""
        if not requested:
            raise UnsupportedTarget(f'Name the device of "{target}" with "--device". {AppleSimulators.DEVICES_HINT}')
        for device in self.physical():
            if requested in (device.identifier, device.name) and target in device.targets:
                return device
        raise UnsupportedTarget(f'No device of "{target}" paired with this Mac has the name or identifier "{requested}". {AppleSimulators.DEVICES_HINT}')
