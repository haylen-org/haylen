from haylen.core.errors.haylen_error import HaylenError


class InvalidOption(HaylenError):
    """An option of a command or of a request to a backend that is missing or has a value it does not take, such as a depot without its numeric ID."""

    code = "invalidOption"
