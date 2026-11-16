from collections.abc import Mapping
from dataclasses import dataclass


@dataclass(frozen=True)
class LaunchRequest:
    """How `app run` asks a backend to launch a built product: the device by the identifier that `haylen devices` lists, or the default one, whether the app plays in development with the development server, the address and port it listens on, the port of the debugger and the options of the command that only some backends read, such as the headers of a web server."""

    device: str | None
    development: bool
    host: str
    port: int
    debugger_port: int
    options: Mapping[str, object]
