import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.report_printer import ReportPrinter
from haylen.core.repository import Repository


class AppVerifyCommand(Command):
    PATH = ("app", "verify")
    HELP = "Verify the last build of an app for a target: the files it made, the targets it serves and what the platform and the plugins need of it."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        pipeline = session.pipeline
        product = pipeline.last_build(pipeline.resolve(Arguments.request(arguments, session)))
        report = pipeline.verify(product)
        ReportPrinter.verification(report, session.terminal, f"`{Repository.shown(product.path)}`")
        return {"product": product.describe(), "verification": report.describe()}
