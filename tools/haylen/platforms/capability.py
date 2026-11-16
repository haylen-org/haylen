from enum import StrEnum


class Capability(StrEnum):
    """An operation that a backend declares for its targets. Preparing, building and verifying are what every backend offers, and the others are optional, so a backend without a device launcher or signing still builds apps."""

    PREPARE = "prepare"
    BUILD = "build"
    VERIFY = "verify"
    RUN = "run"
    DEVICES = "devices"
    PACKAGE = "package"
    SIGN = "sign"
