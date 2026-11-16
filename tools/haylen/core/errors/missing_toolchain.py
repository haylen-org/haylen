from haylen.core.errors.haylen_error import HaylenError


class MissingToolchain(HaylenError):
    """A tool that a target needs and this machine lacks, or has in a version that the target does not accept."""

    code = "missingToolchain"
