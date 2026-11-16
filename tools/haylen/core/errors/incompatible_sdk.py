from haylen.core.errors.haylen_error import HaylenError


class IncompatibleSdk(HaylenError):
    """An engine SDK that is missing for a target, holds no slice that matches it or was built from other engine sources."""

    code = "incompatibleSdk"
