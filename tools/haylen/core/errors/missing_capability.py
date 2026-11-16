from haylen.core.errors.haylen_error import HaylenError


class MissingCapability(HaylenError):
    """An operation, such as running or packaging, that the backend of a target does not declare."""

    code = "missingCapability"
