from haylen.core.errors.haylen_error import HaylenError


class InvalidContribution(HaylenError):
    """A contribution of a plugin or an override of an app that its backend does not accept, or that conflicts with another one."""

    code = "invalidContribution"
