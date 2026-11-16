import argparse

from haylen.core.errors.invalid_option import InvalidOption


class OptionValues:
    """The `--option name=value` arguments of the commands that hand options to a capability of a backend, such as a package format or a signing key. A name given more than once gathers its values into a list, such as the depots of a store."""

    @staticmethod
    def add(parser: argparse.ArgumentParser, help_text: str) -> None:
        parser.add_argument("--option", action="append", default=[], metavar="name=value", help=help_text)

    @staticmethod
    def parse(entries: list[str]) -> dict[str, object]:
        options: dict[str, object] = {}
        for entry in entries:
            name, separator, value = entry.partition("=")
            if not separator or not name:
                raise InvalidOption(f'The option "{entry}" has no value. Give it as "--option name=value".')
            if name not in options:
                options[name] = value
                continue
            earlier = options[name]
            options[name] = [*earlier, value] if isinstance(earlier, list) else [earlier, value]
        return options
