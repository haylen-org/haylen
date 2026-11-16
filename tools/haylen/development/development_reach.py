from enum import StrEnum


class DevelopmentReach(StrEnum):
    """How an app that a backend launches reaches the development server on this machine, which the backend of its target knows and the development tools never guess from a platform. An app on this machine, on a simulator, or on an emulator or a device whose backend forwards the port to this machine reaches it at the loopback address, while a device on the local network reaches it at the address of this machine on that network."""

    LOOPBACK = "loopback"
    NETWORK = "network"
