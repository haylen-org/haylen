from enum import StrEnum


class TargetEnvironment(StrEnum):
    """Where the code of a target runs: on a device of its system, on a simulator or emulator of it, or on the machine that builds it."""

    DEVICE = "device"
    SIMULATOR = "simulator"
    HOST = "host"
