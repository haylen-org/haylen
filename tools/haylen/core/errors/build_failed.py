from haylen.core.errors.haylen_error import HaylenError


class BuildFailed(HaylenError):
    """A native tool that failed while it prepared, built, verified or ran an app or an SDK, with its output as the details."""

    code = "buildFailed"
