import argparse
import json
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.terminal import Terminal


class ProjectInspectCommand(Command):
    PATH = ("project", "inspect")
    HELP = "Show the resolved project of an app for a target: every value with its origin and why it has it, the plugins in load order with their parameters, the merged contributions with their owners and the SDK slice of every architecture."

    PROJECT_COLUMNS = (
        Terminal.Column("Target", style=Terminal.EXPRESSION),
        Terminal.Column("SDK", shrink=2, cut=True),
        Terminal.Column("Minimum system"),
        Terminal.Column("Reason", shrink=1),
    )
    PLUGIN_COLUMNS = (
        Terminal.Column("Plugin", style=Terminal.EXPRESSION),
        Terminal.Column("Version"),
        Terminal.Column("Part"),
        Terminal.Column("Folder", shrink=2, cut=True),
        Terminal.Column("Parameters", shrink=1),
    )
    SETTING_COLUMNS = (
        Terminal.Column("Setting", style=Terminal.EXPRESSION),
        Terminal.Column("Value", shrink=1),
        Terminal.Column("Origin", shrink=2, cut=True),
    )
    CONTRIBUTION_COLUMNS = (
        Terminal.Column("Type", style=Terminal.EXPRESSION),
        Terminal.Column("Owners", shrink=2),
        Terminal.Column("Fields", shrink=1),
    )

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        product = session.pipeline.resolve(Arguments.request(arguments, session), build_sdk=False)
        if not session.structured:
            ProjectInspectCommand.show(session.terminal, product)
        return product.describe()

    @staticmethod
    def show(terminal: Terminal, product: ResolvedProduct) -> None:
        """Prints the projects of every architecture, then the plugins in load order, the settings of the platform sections and the merged contributions of the primary project, each as a group with a table."""
        terminal.group(f"{product.app.name} for {product.target_name}, {product.configuration.value}, with the backend {product.backend}")
        terminal.group("Projects", len(product.projects))
        projects = []
        for project in product.projects:
            minimum = project.minimum_system_version
            projects.append((project.target.text, Repository.shown(project.sdk.manifest.folder), minimum.value, Terminal.plain(f"From {minimum.origin}. {minimum.justification}")))
        terminal.table(ProjectInspectCommand.PROJECT_COLUMNS, projects)

        primary = product.primary
        plugins = [(plugin.id, plugin.manifest.version.text, plugin.implementation.value, Repository.shown(plugin.folder), json.dumps(Frozen.thaw(plugin.parameters))) for plugin in primary.plugins]
        if plugins:
            terminal.group("Plugins", len(plugins))
            terminal.table(ProjectInspectCommand.PLUGIN_COLUMNS, plugins)

        settings = [(f"platforms.{section}.{key}", json.dumps(setting["value"]), product.app.origin) for section, values in primary.setting_origins().items() for key, setting in values.items()]
        if settings:
            terminal.group("Settings", len(settings))
            terminal.table(ProjectInspectCommand.SETTING_COLUMNS, settings)

        contributions = [(contribution.type, ", ".join(contribution.owners), json.dumps(contribution.describe()["fields"])) for contribution in primary.contributions]
        if contributions:
            terminal.group("Contributions", len(contributions))
            terminal.table(ProjectInspectCommand.CONTRIBUTION_COLUMNS, contributions)
