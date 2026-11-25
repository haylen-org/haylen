import ctypes
import re
import time
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.mac_input_sources import MacInputSources


class MacKeyboard:
    """The keyboard of this Mac for the UI tests of the Mac, which ask for what the sandbox of their runner keeps them from doing with a line of their output, which `answer` reads. A test holds a key down, which XCUITest cannot, since it presses a key only once: the run posts the key down, its repeats and the key up into the events of the system at the place where the keyboard posts them, so they reach the app in front as a held key does, each repeat marked as one. A test also presses keys one after the other as a person types, which reach the input method of the Mac as the presses of its keyboard, while XCUITest types the characters of a text, which reach the field without the input method. macOS lets a process post such events only once the person allowed the app that runs it in Accessibility. A test also selects an input source and sets the preferences of input methods, which the run changes through `MacInputSources` until the test asks for the input sources of the Mac back, or the run ends. After each change the run writes the identifier of the input source that it selected, or nothing once the Mac has its input sources back, into a file that the test reads, since the sandbox lets the test read files but keeps the input sources of the Mac from it."""

    # The line of a test that asks to hold a key, with the virtual key code of the Mac and the count of repeats after the first press, which "holdKey" of the tests writes.
    REQUEST = re.compile(r"HaylenHoldKey: (\d+) (\d+)$")
    # The line of a test that asks to press keys one after the other by their virtual key codes, which "pressKeys" of the tests writes.
    PRESSES = re.compile(r"HaylenPressKeys: (\d+(?: \d+)*)$")
    # The line of a test that asks to select an input source by its identifier, and the line that asks for the input sources of the Mac back, which "useInputSource" and the end of the test write.
    INPUT_SOURCE = re.compile(r"HaylenInputSource: (\S+)$")
    RESTORE_INPUT_SOURCES = re.compile(r"HaylenRestoreInputSources$")
    # The line of a test that asks to set a boolean preference of an input method by the domain of its settings and its key, which "setInputMethodPreference" writes.
    INPUT_METHOD_PREFERENCE = re.compile(r"HaylenInputMethodPreference: (\S+) (\S+) (true|false)$")
    CORE_GRAPHICS = "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics"
    CORE_FOUNDATION = "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"
    SERVICES = "/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices"
    # The tap of the events of the hardware, "kCGHIDEventTap", and the field of a key event that marks a repeat, "kCGKeyboardEventAutorepeat".
    HID_TAP = 0
    AUTOREPEAT = 8
    # The time before the first repeat of a held key and between two repeats, the defaults of the keyboard settings of macOS.
    FIRST_REPEAT_SECONDS = 0.375
    REPEAT_SECONDS = 0.09
    # The time that a key stays down and the time until the next key goes down, as a person types.
    TYPING_SECONDS = 0.05

    def __init__(self, status: Path) -> None:
        self.status = status
        self.sources: MacInputSources | None = None
        self.status.write_text("", encoding="utf-8")

    def answer(self, line: str) -> None:
        """Holds the key, presses the keys, selects the input source, sets the preference of an input method or gives the Mac back its input sources when the line asks for it."""
        text = line.strip()
        request = MacKeyboard.REQUEST.search(text)
        if request is not None:
            MacKeyboard.hold(int(request[1]), int(request[2]))
            return

        presses = MacKeyboard.PRESSES.search(text)
        if presses is not None:
            MacKeyboard.press([int(code) for code in presses[1].split()])
            return

        source = MacKeyboard.INPUT_SOURCE.search(text)
        if source is not None:
            self.input_sources().select(source[1])
            self.status.write_text(source[1], encoding="utf-8")
            return

        preference = MacKeyboard.INPUT_METHOD_PREFERENCE.search(text)
        if preference is not None:
            self.input_sources().set_preference(preference[1], preference[2], preference[3] == "true")
            return

        if MacKeyboard.RESTORE_INPUT_SOURCES.search(text) is not None:
            self.restore()

    def input_sources(self) -> MacInputSources:
        if self.sources is None:
            self.sources = MacInputSources()
        return self.sources

    def restore(self) -> None:
        """Gives the Mac back the input sources and the preferences of input methods it had before a test changed them."""
        if self.sources is not None:
            self.sources.restore()
        self.status.write_text("", encoding="utf-8")

    @staticmethod
    def hold(code: int, repeats: int) -> None:
        """Holds the key down for its repeats after the first press, at the pace of the repeats of the keyboard of macOS."""
        presses = [(code, True, False, MacKeyboard.FIRST_REPEAT_SECONDS), *[(code, True, True, MacKeyboard.REPEAT_SECONDS)] * repeats, (code, False, False, 0.0)]
        MacKeyboard.post(presses)

    @staticmethod
    def press(codes: list[int]) -> None:
        """Presses the keys one after the other, each down and up, as a person types."""
        presses = [press for code in codes for press in ((code, True, False, MacKeyboard.TYPING_SECONDS), (code, False, False, MacKeyboard.TYPING_SECONDS))]
        MacKeyboard.post(presses)

    @staticmethod
    def post(presses: list[tuple[int, bool, bool, float]]) -> None:
        """Posts each key going down or up, marked as a repeat or not, into the events of the system where the keyboard posts them, and waits the time after each one."""
        services = ctypes.CDLL(MacKeyboard.SERVICES)
        services.AXIsProcessTrusted.restype = ctypes.c_bool
        if not services.AXIsProcessTrusted():
            raise BuildFailed(
                'A UI test presses keys, which macOS lets the tests post only once the app that runs them is allowed. Allow the app that runs the tests, such as the terminal, in "Accessibility" of "Privacy & Security" in System Settings.'
            )

        graphics = ctypes.CDLL(MacKeyboard.CORE_GRAPHICS)
        graphics.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
        graphics.CGEventCreateKeyboardEvent.argtypes = (ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool)
        graphics.CGEventSetIntegerValueField.argtypes = (ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int64)
        graphics.CGEventPost.argtypes = (ctypes.c_uint32, ctypes.c_void_p)
        foundation = ctypes.CDLL(MacKeyboard.CORE_FOUNDATION)
        foundation.CFRelease.argtypes = (ctypes.c_void_p,)

        for code, down, repeat, pause in presses:
            event = graphics.CGEventCreateKeyboardEvent(None, code, down)
            graphics.CGEventSetIntegerValueField(event, MacKeyboard.AUTOREPEAT, 1 if repeat else 0)
            graphics.CGEventPost(MacKeyboard.HID_TAP, event)
            foundation.CFRelease(event)
            time.sleep(pause)
