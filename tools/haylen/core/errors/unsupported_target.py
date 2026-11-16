from haylen.core.errors.haylen_error import HaylenError


class UnsupportedTarget(HaylenError):
    """A target that no installed backend or engine port declares, or that a backend, an SDK or a plugin of the app does not support."""

    code = "unsupportedTarget"
