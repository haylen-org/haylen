import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.report_printer import ReportPrinter
from haylen.core.repository import Repository


class AppBuildCommand(Command):
    PATH = ("app", "build")
    HELP = 'Prepare the project of an app for a target, build it with the native tools of the target and verify what it built. It never builds the engine, which comes from the SDK that "haylen sdk build" installs.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser)
        Arguments.add_previous_release(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        pipeline = session.pipeline
        product = pipeline.build(pipeline.prepare(pipeline.resolve(Arguments.request(arguments, session, arguments.previous_release))))
        report = pipeline.verify(product)
        ReportPrinter.verification(report, session.terminal, f"`{Repository.shown(product.path)}`")
        return {"product": product.describe(), "verification": report.describe()}
