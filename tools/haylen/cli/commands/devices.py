import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.errors.haylen_error import HaylenError
from haylen.platforms.capability import Capability


class DevicesCommand(Command):
    PATH = ("devices",)
    HELP = 'List the simulators, emulators and devices that the backends of this machine launch apps on, with the identifier that "--device" takes and the targets that run on each.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        return

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        registry = session.registry
        registry.require_healthy()
        listed = []
        missing = []
        for entry in registry.entries:
            if Capability.DEVICES not in entry.description.capabilities:
                continue
            try:
                devices = entry.backend.devices()
            except HaylenError as error:
                missing.append({"backend": entry.description.name, "problem": error.message})
                session.terminal.note(error.message)
                continue
            listed += [{"backend": entry.description.name, **device.describe()} for device in devices]

        rows = [(device["identifier"], f"{device['name']}, {device['kind']}, {device['state']}, for {', '.join(device['targets'])}") for device in listed]
        if rows and not session.structured:
            session.terminal.rows(rows, keys=True)
        if not rows and not session.structured:
            session.terminal.info("No backend found a device, simulator or emulator on this machine.")
        return {"devices": listed, "problems": missing}
