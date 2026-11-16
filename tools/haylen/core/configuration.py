from enum import StrEnum

from haylen.core.errors.unsupported_target import UnsupportedTarget


class Configuration(StrEnum):
    """The configuration of an app or an SDK: Debug ships the package as it is, and Release ships the protected release with the bootstrap of the app."""

    DEBUG = "Debug"
    RELEASE = "Release"

    @staticmethod
    def parse(text: str) -> Configuration:
        for configuration in Configuration:
            if configuration.value == text:
                return configuration
        raise UnsupportedTarget(f'The configuration "{text}" is unknown. It is "Debug" or "Release".')
