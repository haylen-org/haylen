from enum import StrEnum


class DevelopmentReach(StrEnum):
    """How an app that a backend launches reaches the development server on this machine, which the backend of its target knows and the development tools never guess from a platform. An app on this machine, on a simulator, or on an emulator or a device whose backend forwards the port to this machine reaches it at the loopback address. A device that the tools of its platform reach through a tunnel of their own, such as the CoreDevice tunnel of an Apple device over a cable or the network, reaches it at the address of this machine on that tunnel, which is no broadcast-capable network, so no local network permission of the device gates it. Any other device reaches it at the address of this machine on the local network, as `haylen app serve` serves the package to apps on that network."""

    LOOPBACK = "loopback"
    TUNNEL = "tunnel"
    NETWORK = "network"
