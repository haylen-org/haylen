import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.errors.haylen_error import HaylenError
from haylen.platforms.capability import Capability
from haylen.platforms.device import Device
from haylen.terminal import Terminal


class DevicesCommand(Command):
    PATH = ("devices",)
    HELP = 'List the simulators, emulators and devices that the backends of this machine launch apps on, grouped by platform, with their system, their state and the identifier that "--device" takes.'

    # The groups of the listing in the order that developers look for them, each with the systems it holds. A system that no group names, such as one of a backend from outside the repository, gets a group of its own after them.
    GROUPS = (
        ("Android", ("Android",)),
        ("iOS and iPadOS", ("iOS", "iPadOS")),
        ("tvOS", ("tvOS",)),
        ("visionOS", ("visionOS",)),
        ("Desktop", ("macOS", "Windows", "Linux")),
        ("Web", ("Web",)),
    )
    # The states that the backends report, by their sign: ready to launch an app, off until a run starts it, and every other state, which keeps a run from reaching the device.
    READY = frozenset({"ready", "booted", "connected"})
    IDLE = frozenset({"shutdown"})
    COLUMNS = (
        Terminal.Column("Name", shrink=1),
        Terminal.Column("Kind"),
        Terminal.Column("System", shrink=2),
        Terminal.Column("State"),
        Terminal.Column("Identifier", style=Terminal.EXPRESSION, stacks=True),
    )

    def configure(self, parser: argparse.ArgumentParser) -> None:
        return

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        registry = session.registry
        registry.require_healthy()
        found: list[tuple[str, Device]] = []
        missing = []
        for entry in registry.entries:
            if Capability.DEVICES not in entry.description.capabilities:
                continue
            try:
                devices = entry.backend.devices()
            except HaylenError as error:
                missing.append({"backend": entry.description.name, "problem": error.message})
                continue
            found += [(entry.description.name, device) for device in devices]

        if not session.structured:
            devices = [device for _, device in found]
            DevicesCommand.show(session.terminal, devices, [problem["problem"] for problem in missing])
        return {"devices": [{"backend": backend, **device.describe()} for backend, device in found], "problems": missing}

    @staticmethod
    def show(terminal: Terminal, devices: list[Device], problems: list[str]) -> None:
        """Prints a group of every platform with its devices, then what kept a backend from listing its devices."""
        if not devices:
            terminal.info("No backend found a device, simulator or emulator on this machine.")
        groups = [(title, [DevicesCommand.row(device) for device in DevicesCommand.ordered(members)]) for title, members in DevicesCommand.groups(devices)]
        terminal.grouped_table(DevicesCommand.COLUMNS, groups)
        if problems:
            terminal.group("Problems", len(problems))
            for problem in problems:
                terminal.item(Terminal.Sign.FAILED, problem)

    @staticmethod
    def groups(devices: list[Device]) -> list[tuple[str, list[Device]]]:
        """The groups that hold devices, in the order of `GROUPS` and then by the systems that no group names, in the order that the backends listed them."""
        titles = {system: title for title, systems in DevicesCommand.GROUPS for system in systems}
        order = [title for title, _ in DevicesCommand.GROUPS]
        order += list(dict.fromkeys(device.system for device in devices if device.system not in titles))
        members = {title: [device for device in devices if titles.get(device.system, device.system) == title] for title in order}
        return [(title, members[title]) for title in order if members[title]]

    @staticmethod
    def ordered(devices: list[Device]) -> list[Device]:
        """The devices of a group with the physical devices first, then the ones ready for a run, then the newest system first, keeping the order of the backend otherwise."""
        newest = sorted(devices, key=lambda device: DevicesCommand.version_key(device.version), reverse=True)
        signs = list(Terminal.Sign)
        return sorted(newest, key=lambda device: (device.kind != "device", signs.index(DevicesCommand.sign(device.state))))

    @staticmethod
    def version_key(version: str) -> tuple[int, ...]:
        return tuple(int(part) for part in version.split(".") if part.isdigit())

    @staticmethod
    def row(device: Device) -> tuple[str | Terminal.Cell, ...]:
        system = f"{device.system} {device.version}".strip()
        state = Terminal.Cell(device.state.capitalize(), DevicesCommand.sign(device.state))
        return (device.name, device.kind.capitalize(), system, state, device.identifier)

    @staticmethod
    def sign(state: str) -> Terminal.Sign:
        if state in DevicesCommand.READY:
            return Terminal.Sign.READY
        if state in DevicesCommand.IDLE:
            return Terminal.Sign.IDLE
        return Terminal.Sign.UNREACHABLE
