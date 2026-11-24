from haylen.core.errors.haylen_error import HaylenError


class UnsupportedTarget(HaylenError):
    """A target that no installed backend or engine port declares, or that a backend, an SDK or a plugin of the app does not support."""

    code = "unsupportedTarget"
    # The sentence that ends every error about a device, since "--device" takes the identifiers that "haylen devices" lists.
    DEVICES_HINT = 'Use the command "haylen devices" to list the simulators, emulators and devices of this machine.'
