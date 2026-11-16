from enum import StrEnum


class AppKind(StrEnum):
    """The kind of an app, which decides only what its build compiles and loads: a Lua app runs its Lua modules in the player of the engine, and a C++ app compiles its own sources against the engine SDK."""

    LUA = "lua"
    CPP = "cpp"
