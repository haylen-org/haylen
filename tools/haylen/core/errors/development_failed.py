from haylen.core.errors.haylen_error import HaylenError


class DevelopmentFailed(HaylenError):
    """A server of the development tools that cannot start, such as on a port that something else uses or on a machine without a network, a folder or an app that it cannot serve, or a running app that it cannot reach."""

    code = "developmentFailed"
