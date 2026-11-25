class Contract:
    """The versions of the contracts that extensions implement, apart from the version of the engine and of its SDKs. An installed backend or engine port that declares another version stops every command that needs it with a sentence that names both versions, and the registry never tries another extension in its place."""

    BACKEND = 2
    ENGINE_PORT = 1
    BACKEND_GROUP = "haylen.backends"
    ENGINE_PORT_GROUP = "haylen.engine_ports"
