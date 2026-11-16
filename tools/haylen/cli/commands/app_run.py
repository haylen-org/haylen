import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.report_printer import ReportPrinter
from haylen.core.configuration import Configuration
from haylen.core.repository import Repository
from haylen.development.server_options import ServerOptions
from haylen.platforms.capability import Capability
from haylen.platforms.launch_request import LaunchRequest


class AppRunCommand(Command):
    PATH = ("app", "run")
    HELP = 'Prepare, build and verify an app for a target, saying each step as it goes, then launch it: a Debug app plays in development with hot reload and the debugger port, and a Release app plays as it ships. Without "--target" a Lua app plays in the player of this machine and a C++ app builds for this machine. It never builds the engine.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser)
        Arguments.add_previous_release(parser)
        parser.add_argument("--device", help='The identifier of the simulator, emulator or device, which "haylen devices" lists. The default device of the target otherwise.')
        ServerOptions.add_address(parser)
        ServerOptions.add_debugger(parser)
        ServerOptions.add_page(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        pipeline = session.pipeline
        request = Arguments.request(arguments, session, arguments.previous_release)
        resolved = pipeline.resolve(request)
        pipeline.require(resolved, Capability.RUN)
        product = pipeline.build(pipeline.prepare(resolved))
        report = pipeline.verify(product)
        ReportPrinter.verification(report, session.terminal, f"`{Repository.shown(product.path)}`")

        development = request.configuration is Configuration.DEBUG
        launch = LaunchRequest(arguments.device, development, arguments.host, arguments.port, arguments.debugger_port, ServerOptions.page_options(arguments))
        session.terminal.step(f'Launching "{product.prepared.product.app.name}" on "{request.target}"')
        pipeline.run(product, launch)
        return {"product": product.describe(), "verification": report.describe()}
