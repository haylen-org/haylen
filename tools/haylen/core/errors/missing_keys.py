from haylen.core.errors.haylen_error import HaylenError


class MissingKeys(HaylenError):
    """The key folder of an app that holds no keys where the tooling never creates them, such as in continuous integration, which receives the keys of the machine that built the first release."""

    code = "missingKeys"
