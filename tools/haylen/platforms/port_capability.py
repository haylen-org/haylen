from enum import StrEnum


class PortCapability(StrEnum):
    """An operation that an engine port declares beyond building its SDK. Building is what every port offers, and the others are optional."""

    JOIN = "join"
