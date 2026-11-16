import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.option_values import OptionValues
from haylen.cli.report_printer import ReportPrinter
from haylen.core.configuration import Configuration
from haylen.core.repository import Repository
from haylen.platforms.capability import Capability
from haylen.platforms.package_request import PackageRequest
from haylen.platforms.product_folders import ProductFolders


class AppPackageCommand(Command):
    PATH = ("app", "package")
    HELP = 'Build an app for a target, in the Release configuration by default, verify it and package it for distribution in a format of the backend of the target, such as "installer" or "steam" on Windows and Linux, and "bundle" or "apks" on Android.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser, target_required=True, configuration=Configuration.RELEASE)
        Arguments.add_previous_release(parser)
        parser.add_argument("--format", required=True, help="The format of the package, which the backend of the target names.")
        parser.add_argument("-o", "--output", help='The folder of the package, "dist" in the app folder by default.')
        OptionValues.add(parser, 'An option of the format, which repeats, such as "appId=480" and "depots=windows=481" for Steam, with one "depots=<name>=<id>" for every depot.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        pipeline = session.pipeline
        request = Arguments.request(arguments, session, arguments.previous_release)
        resolved = pipeline.resolve(request)
        pipeline.require(resolved, Capability.PACKAGE)
        product = pipeline.build(pipeline.prepare(resolved))
        report = pipeline.verify(product)
        ReportPrinter.verification(report, session.terminal, f"`{Repository.shown(product.path)}`")

        output = Path(arguments.output).expanduser().resolve() if arguments.output else request.app_folder / "dist"
        session.terminal.step(f'Packaging "{product.prepared.product.app.name}" for "{request.target}" as "{arguments.format}"')
        files = pipeline.package(product, PackageRequest(arguments.format, output, OptionValues.parse(arguments.option)))
        for file in files:
            session.terminal.success(f"Packaged `{Repository.shown(Path(file))}`.")
        return {"format": arguments.format, "files": list(files), "build": str(ProductFolders.of(request.app_folder, product.prepared.product.identity).root)}
