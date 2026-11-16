from haylen.core.errors.haylen_error import HaylenError


class MissingRelease(HaylenError):
    """A protected release that a command or a build needs and that was not built yet, or a folder that holds none."""

    code = "missingRelease"
