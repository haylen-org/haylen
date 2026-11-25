import ctypes
import re
import time

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.mac_input_sources import MacInputSources


class MacKeyboard:
    """The keyboard of this Mac for the UI tests of the Mac, which ask for what the sandbox of their runner keeps them from doing with a line of their output, which `answer` reads. A test holds a key down, which XCUITest cannot, since it presses a key only once: the run posts the key down, its repeats and the key up into the events of the system at the place where the keyboard posts them, so they reach the app in front as a held key does, each repeat marked as one. macOS lets a process post such events only once the person allowed the app that runs it in Accessibility. A test also selects an input source, which the run selects through `MacInputSources` until the test asks for the input sources of the Mac back, or the run ends."""

    # The line of a test that asks to hold a key, with the virtual key code of the Mac and the count of repeats after the first press, which "holdKey" of the tests writes.
    REQUEST = re.compile(r"HaylenHoldKey: (\d+) (\d+)$")
    # The line of a test that asks to select an input source by its identifier, and the line that asks for the input sources of the Mac back, which "useInputSource" and the end of the test write.
    INPUT_SOURCE = re.compile(r"HaylenInputSource: (\S+)$")
    RESTORE_INPUT_SOURCES = re.compile(r"HaylenRestoreInputSources$")
    CORE_GRAPHICS = "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics"
    CORE_FOUNDATION = "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"
    SERVICES = "/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices"
    # The tap of the events of the hardware, "kCGHIDEventTap", and the field of a key event that marks a repeat, "kCGKeyboardEventAutorepeat".
    HID_TAP = 0
    AUTOREPEAT = 8
    # The time before the first repeat of a held key and between two repeats, the defaults of the keyboard settings of macOS.
    FIRST_REPEAT_SECONDS = 0.375
    REPEAT_SECONDS = 0.09

    def __init__(self) -> None:
        self.sources: MacInputSources | None = None

    def answer(self, line: str) -> None:
        """Holds the key, selects the input source or gives the Mac back its input sources when the line asks for it."""
        text = line.strip()
        request = MacKeyboard.REQUEST.search(text)
        if request is not None:
            MacKeyboard.hold(int(request[1]), int(request[2]))
            return

        source = MacKeyboard.INPUT_SOURCE.search(text)
        if source is not None:
            if self.sources is None:
                self.sources = MacInputSources()
            self.sources.select(source[1])
            return

        if MacKeyboard.RESTORE_INPUT_SOURCES.search(text) is not None:
            self.restore()

    def restore(self) -> None:
        """Gives the Mac back the input sources it had before a test selected one."""
        if self.sources is not None:
            self.sources.restore()

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
