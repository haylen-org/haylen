from functools import cached_property

from haylen.core.app_pipeline import AppPipeline
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.terminal import Terminal


class CommandSession:
    """What a running command shares with the operations it calls: the context of the tooling, whether the result prints as JSON, and the registry of backends and engine ports, which it discovers once, when a command first needs it."""

    def __init__(self, context: ToolingContext, structured: bool) -> None:
        self.context = context
        self.structured = structured

    @property
    def terminal(self) -> Terminal:
        return self.context.terminal

    @cached_property
    def registry(self) -> BackendRegistry:
        return BackendRegistry.discover(self.context)

    @cached_property
    def pipeline(self) -> AppPipeline:
        return AppPipeline(self.context, self.registry)

    @cached_property
    def sdks(self) -> SdkCatalog:
        return SdkCatalog(self.context.environment)
