from enum import StrEnum


class Implementation(StrEnum):
    """How a plugin works on a platform: its native part runs there and the platform needs it, or its Lua API works alone and native calls answer that no handler exists."""

    NATIVE = "native"
    LUA = "lua"
