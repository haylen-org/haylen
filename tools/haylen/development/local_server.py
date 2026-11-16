import http.server


class LocalServer(http.server.ThreadingHTTPServer):
    """A threaded server of this machine whose listen queue holds every connection a page opens at once, since the queue of five that servers take by default resets the connections beyond it on macOS while a page downloads its files side by side."""

    request_queue_size = 128
    daemon_threads = True
