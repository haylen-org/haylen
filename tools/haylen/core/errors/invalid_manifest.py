from haylen.core.errors.haylen_error import HaylenError


class InvalidManifest(HaylenError):
    """A manifest of an app, a plugin or an SDK that does not follow its schema or names something that does not exist."""

    code = "invalidManifest"
