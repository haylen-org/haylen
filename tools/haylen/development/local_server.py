import http.server
import socket
from collections.abc import Callable


class LocalServer(http.server.ThreadingHTTPServer):
    """A threaded server of this machine whose listen queue holds every connection a page opens at once, since the queue of five that servers take by default resets the connections beyond it on macOS while a page downloads its files side by side. It listens on an IPv6 address too, such as the address of this machine on the tunnel of a device."""

    request_queue_size = 128
    daemon_threads = True

    def __init__(self, address: tuple[str, int], handler: Callable[..., http.server.BaseHTTPRequestHandler]) -> None:
        self.address_family = socket.AF_INET6 if ":" in address[0] else socket.AF_INET
        super().__init__(address, handler)
