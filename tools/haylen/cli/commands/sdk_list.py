import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.terminal import Terminal


class SdkListCommand(Command):
    PATH = ("sdk", "list")
    HELP = "List the installed engine SDKs with their port, configuration, slices and whether they were built from the engine sources of this checkout."

    SDK_COLUMNS = (
        Terminal.Column("Folder", shrink=1, cut=True),
        Terminal.Column("Port"),
        Terminal.Column("Configuration"),
        Terminal.Column("Slices"),
        Terminal.Column("Sources"),
    )
    SLICE_COLUMNS = (
        Terminal.Column("System"),
        Terminal.Column("Environment"),
        Terminal.Column("Architectures", shrink=2),
        Terminal.Column("Toolchain"),
        Terminal.Column("From"),
        Terminal.Column("Graphics APIs", shrink=1),
    )

    def configure(self, parser: argparse.ArgumentParser) -> None:
        return

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        revision = Repository.engine_revision()
        survey = session.sdks.survey()
        sentences = {unusable.folder: unusable.sentence(session.registry.targets_named_by(unusable.slices)) for unusable in survey.unusable}
        if not session.structured:
            SdkListCommand.show(session.terminal, survey, revision, sentences)
        sdks = [{**manifest.describe(), "folder": str(manifest.folder), "current": manifest.revision == revision} for manifest in survey.manifests]
        unusable = [{"folder": str(entry.folder), "readable": entry.readable, "problem": sentences[entry.folder]} for entry in survey.unusable]
        return {"sdks": sdks, "unusable": unusable}

    @staticmethod
    def show(terminal: Terminal, survey: SdkCatalog.Survey, revision: str, sentences: Mapping[Path, str]) -> None:
        """Prints a table of the SDKs with whether the engine sources of this checkout built them, a group of the slices of every SDK, and a group for every SDK that this tooling cannot use, with what happened and the command that builds it again."""
        if not survey.manifests and not survey.unusable:
            terminal.info('No SDK is installed. Build one with "haylen sdk build --target <target>".')
            return
        rows = [SdkListCommand.sdk_row(manifest, revision) for manifest in survey.manifests]
        rows += [(Repository.shown(unusable.folder), "", "", "", Terminal.Cell(unusable.state, Terminal.Sign.UNREACHABLE)) for unusable in survey.unusable]
        terminal.group("SDKs", len(rows))
        terminal.table(SdkListCommand.SDK_COLUMNS, rows)
        groups = [(Repository.shown(manifest.folder), [SdkListCommand.slice_row(entry) for entry in manifest.slices]) for manifest in survey.manifests]
        terminal.grouped_table(SdkListCommand.SLICE_COLUMNS, groups)
        for unusable in survey.unusable:
            terminal.group(Repository.shown(unusable.folder))
            terminal.item(Terminal.Sign.UNREACHABLE, sentences[unusable.folder])

    @staticmethod
    def sdk_row(manifest: SdkManifest, revision: str) -> tuple[str | Terminal.Cell, ...]:
        current = Terminal.Cell("Current", Terminal.Sign.READY)
        other = Terminal.Cell("Other sources", Terminal.Sign.UNREACHABLE)
        sources = current if manifest.revision == revision else other
        return (Repository.shown(manifest.folder), f"{manifest.port} {manifest.port_version}", manifest.configuration.value, str(len(manifest.slices)), sources)

    @staticmethod
    def slice_row(entry: SdkSlice) -> tuple[str, ...]:
        return (entry.system, entry.environment.value, ", ".join(entry.architectures), entry.toolchain, entry.minimum_system_version, ", ".join(entry.graphics_apis))
