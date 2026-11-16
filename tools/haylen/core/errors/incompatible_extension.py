from haylen.core.errors.haylen_error import HaylenError


class IncompatibleExtension(HaylenError):
    """An installed backend or engine port that implements another version of its contract, or that does not load."""

    code = "incompatibleExtension"
