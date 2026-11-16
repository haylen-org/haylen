import argparse

from haylen.cli.command_session import CommandSession
from haylen.content.protected_releases import ProtectedReleases
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.backend_registry import BackendRegistry


class ContentArguments:
    """What the content commands share: the target whose content profile a release is built for, the protected releases of the content tool and the report of the tool, which prints as it is and returns as its lines."""

    @staticmethod
    def add_target(parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--target",
            required=True,
            help='The target whose content profile the release serves, such as "ios" or "android", which "haylen doctor" lists. The targets of one store share a profile.',
        )

    @staticmethod
    def profile(arguments: argparse.Namespace, session: CommandSession) -> tuple[BackendRegistry.Target, str]:
        """Returns the target that the command names with the content profile that its backend declares for it."""
        target = session.registry.target(arguments.target)
        profile = target.description.release_profile
        if not profile:
            raise UnsupportedTarget(f'The target "{target.description.name}" ships no release, so it has no protected release. Name a target that ships apps.')
        return target, profile

    @staticmethod
    def releases(session: CommandSession) -> ProtectedReleases:
        return ProtectedReleases.locate(session.context, session.registry)

    @staticmethod
    def report(session: CommandSession, text: str) -> list[str]:
        """Prints the report of the content tool in the terminal, and returns its lines for structured output."""
        lines = text.rstrip("\n").splitlines()
        if not session.structured:
            session.terminal.verbatim("\n".join(lines))
        return lines
