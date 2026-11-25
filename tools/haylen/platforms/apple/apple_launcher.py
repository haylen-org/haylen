import json
import re
import signal
import subprocess

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.tooling_context import ToolingContext
from haylen.development.app_run import AppRun
from haylen.development.development_reach import DevelopmentReach
from haylen.engine.unified_log import UnifiedLog
from haylen.platforms.apple.app_bundle import AppBundle
from haylen.platforms.apple.apple_devices import AppleDevices
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.ios_devices import IosDevices
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.launch_request import LaunchRequest
from haylen.toolchains.apple_simulators import AppleSimulators


class AppleLauncher:
    """Launches a built Apple app on this Mac, a simulator or a device and follows it until it ends or the developer quits. The engine writes to the standard output of a macOS app and to the unified log elsewhere, which the launcher relays next to the output of the process. A development launch starts the app with the frame capture of Metal, which simulators take through variables that start with `SIMCTL_CHILD_` and devices through the variables of devicectl."""

    CAPTURE = {"MTL_CAPTURE_ENABLED": "1"}
    # A device gives the Mac no stream of its unified log, so the launch asks the process to mirror its records to its error output, which devicectl relays.
    DEVICE_LOG = {"OS_ACTIVITY_DT_MODE": "YES"}
    # A record that the unified log mirrors to the output of a process: its time, the process with its identifiers, the category in brackets and the message.
    DEVICE_RECORD = re.compile(r"\d{4}-\d{2}-\d{2} [\d:.]+[+-]\d{4} .+?\[\d+:\d+\] (?:\[(?P<category>[^\]]*)\] )?(?P<message>.*)")

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, product: BuildProduct, launch: LaunchRequest, target: AppleTarget) -> None:
        bundle = AppBundle(self.context, product.path, target.runs_on_mac)
        if target.runs_on_mac:
            self.run_on_mac(product, launch, target, bundle)
        elif target.simulator:
            self.run_on_simulator(product, launch, target, bundle)
        else:
            self.run_on_device(product, launch, target, bundle)

    def run_on_mac(self, product: BuildProduct, launch: LaunchRequest, target: AppleTarget, bundle: AppBundle) -> None:
        with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, bundle.package) as run:
            environment = AppleLauncher.CAPTURE if run.developing else None
            command = [bundle.executable, *run.arguments]
            if target.name == "macos":
                run.show("This Mac")
                run.follow(command, environment=environment)
                return
            with UnifiedLog(self.context.terminal, bundle.executable.name, []):
                run.show("This Mac, with Mac Catalyst")
                run.follow(command, environment=environment)

    def run_on_simulator(self, product: BuildProduct, launch: LaunchRequest, target: AppleTarget, bundle: AppBundle) -> None:
        with AppleSimulators(self.context).booted(str(target.family), launch.device, IosDevices.of(product.prepared.product.app)) as simulator:
            udid = simulator["udid"]
            self.context.terminal.step(f'Installing the app on the simulator "{simulator["name"]}"')
            self.context.process.run(["xcrun", "simctl", "install", udid, product.path])
            with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, bundle.package) as run, UnifiedLog(self.context.terminal, bundle.executable.name, ["xcrun", "simctl", "spawn", udid]):
                environment = {f"SIMCTL_CHILD_{name}": value for name, value in AppleLauncher.CAPTURE.items()} if run.developing else None
                identifier = bundle.identifier
                run.show(f"{simulator['name']}, simulator", lambda: AppleLauncher.terminate(udid, identifier))
                command = ["xcrun", "simctl", "launch", "--console-pty", "--terminate-running-process", udid, identifier, *run.arguments]
                run.follow(command, line=lambda line: AppleLauncher.simulator_line(identifier, line), environment=environment)

    def run_on_device(self, product: BuildProduct, launch: LaunchRequest, target: AppleTarget, bundle: AppBundle) -> None:
        device = AppleDevices(self.context).device(launch.device, target.name)
        app = product.prepared.product.app
        if target.folder == "ios" and device.system != AppleDevices.IPAD and IosDevices.of(app) == (IosDevices.IPAD,):
            raise UnsupportedTarget(
                f'The app runs on iPads alone, as "platforms.ios.devices" of `{app.origin}` says, so it does not install on the iPhone "{device.name}". Name an iPad with "--device".',
                origin=app.origin,
            )
        self.context.terminal.step(f'Installing the app on "{device.name}"')
        self.context.process.run(["xcrun", "devicectl", "device", "install", "app", "--device", device.identifier, product.path])
        tunnel = AppleDevices(self.context).tunnel_address(device) if launch.development else None
        with AppRun(self.context, product, launch, DevelopmentReach.TUNNEL, bundle.package, tunnel) as run:
            environment = {**AppleLauncher.DEVICE_LOG, **AppleLauncher.CAPTURE} if run.developing else AppleLauncher.DEVICE_LOG
            variables = ["--environment-variables", json.dumps(environment)]
            # The launch ends the app when it stops, as Ctrl+C does.
            run.show(f"{device.name}, device", lambda: run.process.send_signal(signal.SIGINT))
            command = ["xcrun", "devicectl", "device", "process", "launch", "--console", "--device", device.identifier, *variables, bundle.identifier, *run.arguments]
            run.follow(command, line=AppleLauncher.device_line)

    @staticmethod
    def terminate(udid: str, identifier: str) -> None:
        subprocess.run(["xcrun", "simctl", "terminate", udid, identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)

    @staticmethod
    def simulator_line(identifier: str, line: str) -> str | None:
        """Leaves out the lines that the launcher of a simulator adds to the output of the app: the process it started and the signal that ended it."""
        if re.fullmatch(rf"{re.escape(identifier)}: \d+", line.strip()) or line.startswith("Child process terminated with signal"):
            return None
        return line

    @staticmethod
    def device_line(line: str) -> str | None:
        """Keeps the message of each record that the engine wrote to the unified log of a device, and leaves out the other records that the process mirrors to its output, such as those of UIKit. Lines that are not records of the unified log, such as those of devicectl and the rest of a record of many lines, stay as they are."""
        record = AppleLauncher.DEVICE_RECORD.fullmatch(line.rstrip("\r\n"))
        if record is None:
            return line
        if record["category"] != UnifiedLog.CATEGORY:
            return None
        return record["message"]
