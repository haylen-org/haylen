import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository


class SdkListCommand(Command):
    PATH = ("sdk", "list")
    HELP = "List the installed engine SDKs with their port, configuration, slices and whether they were built from the engine sources of this checkout."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        return

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        revision = Repository.engine_revision()
        manifests = session.sdks.manifests()
        listed = []
        for manifest in manifests:
            current = manifest.revision == revision
            listed.append({**manifest.describe(), "folder": str(manifest.folder), "current": current})
            if session.structured:
                continue
            state = "current" if current else "built from other engine sources"
            session.terminal.heading(f"`{Repository.shown(manifest.folder)}`, {manifest.configuration.value}, {state}")
            for entry in manifest.slices:
                apis = ", ".join(entry.graphics_apis) or "no graphics API"
                session.terminal.info(f'  "{entry.system}" {entry.environment.value} {", ".join(entry.architectures)} with "{entry.toolchain}", from {entry.minimum_system_version}, {apis}')
        if not manifests and not session.structured:
            session.terminal.info('No SDK is installed. Build one with "haylen sdk build --target <target>".')
        return {"sdks": listed}
