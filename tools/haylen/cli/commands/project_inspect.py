import argparse
import json
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.frozen import Frozen


class ProjectInspectCommand(Command):
    PATH = ("project", "inspect")
    HELP = "Show the resolved project of an app for a target: every value with its origin and why it has it, the plugins in load order with their parameters, the merged contributions with their owners and the SDK slice of every architecture."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        product = session.pipeline.resolve(Arguments.request(arguments, session))
        described = product.describe()
        if session.structured:
            return described

        terminal = session.terminal
        terminal.heading(f"{product.app.name} for {product.target_name} ({product.configuration.value})")
        for project in product.projects:
            minimum = project.minimum_system_version
            terminal.rows(
                [
                    ("Target", project.target.text),
                    ("Backend", product.backend),
                    ("SDK", f"`{project.sdk.manifest.folder}`"),
                    ("Minimum system", f'"{minimum.value}" from {minimum.origin}. {minimum.justification}'),
                ]
            )
        primary = product.primary
        terminal.heading("Plugins")
        for plugin in primary.plugins:
            parameters = json.dumps(Frozen.thaw(plugin.parameters))
            terminal.info(f'  "{plugin.id}" {plugin.manifest.version.text}, {plugin.implementation.value}, from `{plugin.folder}`, with {parameters}')
        terminal.heading("Settings")
        for section, values in primary.setting_origins().items():
            for key, setting in values.items():
                terminal.info(f'  {json.dumps(setting["value"])} from `{product.app.origin}` at "platforms.{section}.{key}"')
        terminal.heading("Contributions")
        for contribution in primary.contributions:
            owners = ", ".join(f'"{owner}"' for owner in contribution.owners)
            terminal.info(f'  "{contribution.type}" from {owners}: {json.dumps(contribution.describe()["fields"])}')
        return described
