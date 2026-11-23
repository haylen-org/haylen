import functools
import subprocess
import threading
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.tooling_context import ToolingContext
from haylen.development.development_address import DevelopmentAddress
from haylen.development.development_handler import DevelopmentHandler
from haylen.development.development_reach import DevelopmentReach
from haylen.development.development_server import DevelopmentServer
from haylen.development.local_server import LocalServer
from haylen.development.run_session import RunSession
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.launch_request import LaunchRequest


class AppRun:
    """The run of a built app that a backend launches from `PlatformBackend.run` on this machine, a simulator, an emulator or a device, outside a browser. The backend installs and starts the app and follows its log through the run, and the run brings what every target shares: the summary and the keys of the terminal and, in development, the development server that the app connects to with hot reload of the app and its plugins, the developer console, GPU captures and the debugger of an editor. The backend tells how its app reaches this machine, since only it knows, and the run never names a platform.

    Use it as a context manager. In development, entering it starts the server on the port of the launch, the debugger relay on the debugger port and the record that `haylen app capture` reads, and leaving it stops all of them. A failure or a signal inside the block also stops the process that the run follows. Without development, such as for a release, it starts nothing and offers only the keys that need no server.

    ```python
    with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, package) as run:
        run.show("This Mac, in the player")
        run.follow([player, *run.arguments, package])
    ```
    """

    def __init__(self, context: ToolingContext, product: BuildProduct, launch: LaunchRequest, reach: DevelopmentReach, shipped: Path, peer: str | None = None) -> None:
        """Takes the built product, the launch that `app run` asks for, how the app reaches this machine, with the address of the device on its tunnel for `DevelopmentReach.TUNNEL`, and the package folder as the build ships it, whose files the server compares the saved files with, so the app catches up on what changed since the build."""
        self.context = context
        self.product = product
        self.launch = launch
        self.reach = reach
        self.peer = peer
        self.shipped = shipped
        self.development: DevelopmentServer | None = None
        self.listener: LocalServer | None = None
        self.session = RunSession(context)

    def __enter__(self) -> AppRun:
        if not self.launch.development:
            return self

        resolved = self.product.prepared.product
        address = DevelopmentAddress.of(self.reach, self.launch.host, self.peer)
        development = DevelopmentServer(self.context, resolved.package, DevelopmentServer.folder_manifest(self.shipped), resolved.primary.sdk)
        handler = functools.partial(DevelopmentHandler, development=development)
        try:
            listener = LocalServer((address.listen, self.launch.port), handler)
        except OSError as error:
            raise DevelopmentFailed(
                f'The development server cannot listen on port {self.launch.port} of "{address.listen}": {error.strerror}. Stop what uses the port, or pick another one with "--port".'
            ) from error

        port = listener.server_address[1]
        threading.Thread(target=listener.serve_forever, daemon=True).start()
        try:
            # Only an app that runs Lua has a debugger, of its Lua.
            development.start(address.listen, port, self.launch.debugger_port if resolved.app.runs_lua else None)
        except DevelopmentFailed:
            listener.shutdown()
            listener.server_close()
            raise

        self.development = development
        self.listener = listener
        self.session = RunSession(self.context, development, address.websocket(port, development.token))
        return self

    def __exit__(self, failure: type[BaseException] | None, *_: object) -> None:
        self.session.end(failure is not None)
        if self.development is not None:
            self.development.close()
        if self.listener is not None:
            self.listener.shutdown()
            self.listener.server_close()

    @property
    def developing(self) -> bool:
        """Whether the app plays in development, connected to the development server of the run."""
        return self.development is not None

    @property
    def address(self) -> str | None:
        """The WebSocket address of the development server as the app reaches it, which a backend hands to an app that takes no command line, such as in an extra of an Android intent."""
        return self.session.address

    @property
    def arguments(self) -> list[str]:
        """The arguments that connect the app to the development server on its command line, or none without development."""
        return self.session.arguments

    @property
    def port(self) -> int | None:
        """The port that the development server listens on, which a backend forwards to an emulator or a device that reaches this machine through its loopback address."""
        return self.listener.server_address[1] if self.listener is not None else None

    @property
    def process(self) -> subprocess.Popen | None:
        """The process that the run follows, for a stop that signals it."""
        return self.session.process

    @property
    def quitting(self) -> threading.Event:
        """Set once the developer quits or the run ends, for a backend that watches its app on the side, such as whether its process still runs."""
        return self.session.quitting

    def rows(self, device: str) -> list[tuple[str, str]]:
        """The summary of the running app: its name and identifier, the device it runs on, its configuration and, in development, how its files reach it and where an editor attaches its debugger."""
        resolved = self.product.prepared.product
        configuration = f'"{resolved.configuration.value}", with the engine in "{resolved.request.engine_configuration.value}"'
        rows = [("App", f'{resolved.app.name} ("{resolved.app.identifier}")'), ("Device", device), ("Configuration", configuration)]
        if self.development is not None:
            rows.append(("Hot reload", "Every file saved in the app reloads in it"))
        if self.development is not None and self.development.debugger is not None:
            rows.append(("Debugger", f"Attach an editor to port {self.development.debugger.port} of this machine"))
        return rows

    def show(self, device: str, stop: Callable[[], None] | None = None) -> None:
        """Prints the summary of the app on the device, as the backend names it, and the keys, and takes the keys from then on. The key "q" calls the stop of the backend, such as one that ends the app on a simulator, or else ends the process that the run follows."""
        self.session.show(self.rows(device), stop)

    def follow(
        self,
        command: Sequence[object],
        *,
        cwd: Path | None = None,
        line: Callable[[str], str | None] | None = None,
        environment: Mapping[str, str] | None = None,
    ) -> None:
        """Starts the process that runs the app, or that relays its log, and streams its output above the line of the console until it ends. The function of the line rewrites each line or leaves it out by returning nothing, and the variables of the environment add to those of the command. A process that fails is a `BuildFailed`, unless the developer quit."""
        self.session.follow(command, cwd=cwd, line=line, environment=environment)

    def quit(self) -> None:
        self.session.quit()
