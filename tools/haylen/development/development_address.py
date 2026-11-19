import socket
from dataclasses import dataclass

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.development_reach import DevelopmentReach
from haylen.development.development_server import DevelopmentServer


@dataclass(frozen=True)
class DevelopmentAddress:
    """Where the development server of a run listens and the address at which the app reaches this machine. An app on the loopback address reaches the server on the host of the command, a device behind a tunnel reaches it on the address of this machine on that tunnel, and a device on the local network reaches it on every address of this machine, unless the host names one."""

    LOOPBACK = "127.0.0.1"
    EVERY_ADDRESS = "0.0.0.0"
    # An address of the documentation range that no network routes, which a UDP socket connects to without sending anything.
    PROBE = ("192.0.2.1", 9)

    listen: str
    reached: str

    @staticmethod
    def of(reach: DevelopmentReach, host: str, peer: str | None = None) -> DevelopmentAddress:
        """The address for an app that reaches this machine as `reach` says, where `peer` is the address of the device on its tunnel for `DevelopmentReach.TUNNEL`."""
        if reach is DevelopmentReach.TUNNEL and host == DevelopmentAddress.LOOPBACK:
            local = DevelopmentAddress.toward(str(peer))
            return DevelopmentAddress(local, local)

        network = reach is DevelopmentReach.NETWORK
        listen = DevelopmentAddress.EVERY_ADDRESS if network and host == DevelopmentAddress.LOOPBACK else host
        if listen != DevelopmentAddress.EVERY_ADDRESS:
            return DevelopmentAddress(listen, listen)
        reached = DevelopmentAddress.local_network() if network else DevelopmentAddress.LOOPBACK
        return DevelopmentAddress(listen, reached)

    @staticmethod
    def local_network() -> str:
        """Returns the address of this machine on the network that reaches other networks, which devices on the same network connect to, and fails when this machine is on no such network."""
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            try:
                probe.connect(DevelopmentAddress.PROBE)
            except OSError as error:
                raise DevelopmentFailed(f'This machine has no address on a network that devices reach: {error.strerror}. Join a network, or name the address with "--host".') from error
            return probe.getsockname()[0]

    @staticmethod
    def toward(peer: str) -> str:
        """Returns the address of this machine on the route to a device, such as its address on the tunnel of the device, which a UDP socket finds without sending anything."""
        family = socket.AF_INET6 if ":" in peer else socket.AF_INET
        with socket.socket(family, socket.SOCK_DGRAM) as probe:
            try:
                probe.connect((peer, DevelopmentAddress.PROBE[1]))
            except OSError as error:
                raise DevelopmentFailed(f'This machine has no route to the device at "{peer}": {error.strerror}. Connect the device again, by cable or on the network, as Xcode needs it.') from error
            return probe.getsockname()[0]

    def websocket(self, port: int, token: str) -> str:
        """The address of the development path that the app connects to, with the token of the run, and an IPv6 address in brackets."""
        host = f"[{self.reached}]" if ":" in self.reached else self.reached
        return f"ws://{host}:{port}{DevelopmentServer.PATH}?token={token}"
