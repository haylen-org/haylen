import argparse


class ServerOptions:
    """The options of the commands that serve an app or a site, with the same names and defaults everywhere, and the keys of `LaunchRequest.options` that carry the headers of a site and whether to open its page."""

    PORT = 8000
    DEBUGGER_PORT = 4711
    OFF = "off"
    COEP = "coep"
    COOP = "coop"
    OPEN = "open"
    EMBEDDER_POLICIES = ("require-corp", "credentialless", OFF)
    OPENER_POLICIES = (OFF, "same-origin-allow-popups", "same-origin")
    RUN_HOST_HELP = 'Address that the web server of a web page and the development server of a Debug run listen on. The address of this machine on the local network lets phones and other computers open a page, which then runs without sound, because browsers offer "AudioWorklet" only to pages served over https or from "localhost". A device on the local network reaches the development server on every address of this machine unless it names one.'
    RUN_PORT_HELP = "Port of the web server of a web page and of the development server of a Debug run,"

    @staticmethod
    def add_address(parser: argparse.ArgumentParser, host_help: str = RUN_HOST_HELP, port_help: str = RUN_PORT_HELP) -> None:
        """Adds `--host` and `--port`, with the help of `app run` unless a command describes them its own way. The help of the port ends with its default."""
        parser.add_argument("--host", default="127.0.0.1", help=host_help)
        parser.add_argument("--port", type=int, default=ServerOptions.PORT, help=f"{port_help} {ServerOptions.PORT} by default.")

    @staticmethod
    def add_debugger(parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--debugger-port",
            type=int,
            default=ServerOptions.DEBUGGER_PORT,
            help=f"Port of this machine that an editor attaches its debugger to with the Debug Adapter Protocol, in the Debug configuration, {ServerOptions.DEBUGGER_PORT} by default. The development server carries the messages of the editor to the app.",
        )

    @staticmethod
    def add_page(parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--coep",
            default=ServerOptions.OFF,
            choices=ServerOptions.EMBEDDER_POLICIES,
            help='The "Cross-Origin-Embedder-Policy" header of a web page, none by default, since the single-threaded runtime needs no cross-origin isolation and "require-corp" blocks the third-party scripts of plugins, such as those of ads and sign-in, which send no resource policy. A policy with "--coop same-origin" isolates the page for "SharedArrayBuffer" and threads.',
        )
        parser.add_argument(
            "--coop",
            default=ServerOptions.OFF,
            choices=ServerOptions.OPENER_POLICIES,
            help='The "Cross-Origin-Opener-Policy" header of a web page, none by default, since the single-threaded runtime needs no cross-origin isolation and "same-origin" cuts the page off from the sign-in and payment popups of plugins. The value "same-origin" with a "--coep" policy isolates the page for "SharedArrayBuffer" and threads.',
        )
        parser.add_argument("--open", action="store_true", help="Open the web page in the default browser.")

    @staticmethod
    def page_options(arguments: argparse.Namespace) -> dict[str, object]:
        """The options of a launch that the web backend reads from the options of `add_page`."""
        return {ServerOptions.COEP: arguments.coep, ServerOptions.COOP: arguments.coop, ServerOptions.OPEN: arguments.open}
