import ctypes
import re
import time

from haylen.core.errors.build_failed import BuildFailed


class MacKeyboard:
    """Holds a key of this Mac down for the UI tests of the Mac, which XCUITest cannot, since it presses a key only once. A test asks with a line of its output, which `answer` reads, and the run posts the key down, its repeats and the key up into the events of the system at the place where the keyboard posts them, so they reach the app in front as a held key does, each repeat marked as one. macOS lets a process post such events only once the person allowed the app that runs it in Accessibility."""

    # The line of a test that asks to hold a key, with the virtual key code of the Mac and the count of repeats after the first press, which "holdKey" of the tests writes.
    REQUEST = re.compile(r"HaylenHoldKey: (\d+) (\d+)$")
    CORE_GRAPHICS = "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics"
    CORE_FOUNDATION = "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"
    SERVICES = "/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices"
    # The tap of the events of the hardware, "kCGHIDEventTap", and the field of a key event that marks a repeat, "kCGKeyboardEventAutorepeat".
    HID_TAP = 0
    AUTOREPEAT = 8
    # The time before the first repeat of a held key and between two repeats, the defaults of the keyboard settings of macOS.
    FIRST_REPEAT_SECONDS = 0.375
    REPEAT_SECONDS = 0.09

    @staticmethod
    def answer(line: str) -> None:
        """Holds the key when the line asks for it."""
        request = MacKeyboard.REQUEST.search(line.strip())
        if request is not None:
            MacKeyboard.hold(int(request[1]), int(request[2]))

    @staticmethod
    def hold(code: int, repeats: int) -> None:
        services = ctypes.CDLL(MacKeyboard.SERVICES)
        services.AXIsProcessTrusted.restype = ctypes.c_bool
        if not services.AXIsProcessTrusted():
            raise BuildFailed(
                'A UI test holds a key, which macOS lets the tests post only once the app that runs them is allowed. Allow the app that runs the tests, such as the terminal, in "Accessibility" of "Privacy & Security" in System Settings.'
            )

        graphics = ctypes.CDLL(MacKeyboard.CORE_GRAPHICS)
        graphics.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
        graphics.CGEventCreateKeyboardEvent.argtypes = (ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool)
        graphics.CGEventSetIntegerValueField.argtypes = (ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int64)
        graphics.CGEventPost.argtypes = (ctypes.c_uint32, ctypes.c_void_p)
        foundation = ctypes.CDLL(MacKeyboard.CORE_FOUNDATION)
        foundation.CFRelease.argtypes = (ctypes.c_void_p,)

        presses = [(True, False), *[(True, True)] * repeats, (False, False)]
        pauses = [MacKeyboard.FIRST_REPEAT_SECONDS, *[MacKeyboard.REPEAT_SECONDS] * repeats, 0.0]
        for (down, repeat), pause in zip(presses, pauses, strict=True):
            event = graphics.CGEventCreateKeyboardEvent(None, code, down)
            graphics.CGEventSetIntegerValueField(event, MacKeyboard.AUTOREPEAT, 1 if repeat else 0)
            graphics.CGEventPost(MacKeyboard.HID_TAP, event)
            foundation.CFRelease(event)
            time.sleep(pause)
