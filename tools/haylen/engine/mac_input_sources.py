import ctypes

from haylen.core.errors.build_failed import BuildFailed


class MacInputSources:
    """Selects an input source of this Mac for the UI tests of the Mac, through Text Input Sources of Carbon, and sets the preferences of input methods, through the preferences of Core Foundation, and gives the Mac back the input sources and the preferences it had. A test runs in the sandbox of macOS, which keeps the input sources that it turns on or selects to its own process and the settings of the input methods from it, so the run, which runs outside the sandbox, changes them for it. An input mode, such as the Japanese one that composes kana from Latin letters, is selected only once it and its input method are on, so the run turns both on when the Mac has them off and off again when it gives the Mac back its input sources."""

    CARBON = "/System/Library/Frameworks/Carbon.framework/Carbon"
    CORE_FOUNDATION = "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"
    # The encoding of UTF-8 of Core Foundation, "kCFStringEncodingUTF8", and the status of a call of Carbon that succeeded, "noErr".
    UTF8 = 0x08000100
    NO_ERROR = 0
    # The longest identifier of an input source that the run reads.
    IDENTIFIER_SIZE = 256

    def __init__(self) -> None:
        self.carbon = ctypes.CDLL(MacInputSources.CARBON)
        self.foundation = ctypes.CDLL(MacInputSources.CORE_FOUNDATION)
        MacInputSources.declare(self.carbon, self.foundation)
        # The input source that was current before the first selection, and the input sources that the run turned on, in the order it turned them on, each retained until the run gives them back.
        self.previous: int | None = None
        self.enabled: list[int] = []
        # The domain, the key and the value that each preference that the run set had before, in the order the run set them, each value retained until the run gives it back, or None for a preference that the Mac did not hold.
        self.preferences: list[tuple[str, str, int | None]] = []

    @staticmethod
    def declare(carbon: ctypes.CDLL, foundation: ctypes.CDLL) -> None:
        """Declares the functions of Carbon and Core Foundation that the run calls, whose references are pointers and whose statuses are integers of 32 bits."""
        pointer = ctypes.c_void_p
        for name, result, arguments in (
            ("TISCreateInputSourceList", pointer, (pointer, ctypes.c_bool)),
            ("TISCopyCurrentKeyboardInputSource", pointer, ()),
            ("TISGetInputSourceProperty", pointer, (pointer, pointer)),
            ("TISEnableInputSource", ctypes.c_int32, (pointer,)),
            ("TISDisableInputSource", ctypes.c_int32, (pointer,)),
            ("TISSelectInputSource", ctypes.c_int32, (pointer,)),
        ):
            function = getattr(carbon, name)
            function.restype = result
            function.argtypes = arguments
        for name, result, arguments in (
            ("CFStringCreateWithCString", pointer, (pointer, ctypes.c_char_p, ctypes.c_uint32)),
            ("CFStringGetCString", ctypes.c_bool, (pointer, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32)),
            ("CFDictionaryCreate", pointer, (pointer, ctypes.POINTER(pointer), ctypes.POINTER(pointer), ctypes.c_long, pointer, pointer)),
            ("CFArrayGetCount", ctypes.c_long, (pointer,)),
            ("CFArrayGetValueAtIndex", pointer, (pointer, ctypes.c_long)),
            ("CFBooleanGetValue", ctypes.c_bool, (pointer,)),
            ("CFRetain", pointer, (pointer,)),
            ("CFRelease", None, (pointer,)),
            ("CFPreferencesCopyAppValue", pointer, (pointer, pointer)),
            ("CFPreferencesSetAppValue", None, (pointer, pointer, pointer)),
            ("CFPreferencesAppSynchronize", ctypes.c_bool, (pointer,)),
        ):
            function = getattr(foundation, name)
            function.restype = result
            function.argtypes = arguments

    def constant(self, name: str) -> int:
        """A constant string of Carbon, such as the key of a property of an input source."""
        return ctypes.c_void_p.in_dll(self.carbon, name).value

    def string(self, text: str) -> int:
        return self.foundation.CFStringCreateWithCString(None, text.encode("utf-8"), MacInputSources.UTF8)

    def text(self, value: int) -> str:
        buffer = ctypes.create_string_buffer(MacInputSources.IDENTIFIER_SIZE)
        if not self.foundation.CFStringGetCString(value, buffer, MacInputSources.IDENTIFIER_SIZE, MacInputSources.UTF8):
            return ""
        return buffer.value.decode("utf-8")

    def find(self, properties: dict[str, int]) -> int | None:
        """The first installed input source whose properties, named by their constants of Carbon, have the values, retained, or None when the Mac has none."""
        count = len(properties)
        keys = (ctypes.c_void_p * count)(*(self.constant(name) for name in properties))
        values = (ctypes.c_void_p * count)(*properties.values())
        key_callbacks = ctypes.addressof(ctypes.c_char.in_dll(self.foundation, "kCFTypeDictionaryKeyCallBacks"))
        value_callbacks = ctypes.addressof(ctypes.c_char.in_dll(self.foundation, "kCFTypeDictionaryValueCallBacks"))
        query = self.foundation.CFDictionaryCreate(None, keys, values, count, key_callbacks, value_callbacks)
        sources = self.carbon.TISCreateInputSourceList(query, True)
        self.foundation.CFRelease(query)
        if not sources:
            return None
        found = self.foundation.CFRetain(self.foundation.CFArrayGetValueAtIndex(sources, 0)) if self.foundation.CFArrayGetCount(sources) > 0 else None
        self.foundation.CFRelease(sources)
        return found

    def property(self, source: int, name: str) -> int | None:
        return self.carbon.TISGetInputSourceProperty(source, self.constant(name))

    def is_enabled(self, source: int) -> bool:
        value = self.property(source, "kTISPropertyInputSourceIsEnabled")
        return value is not None and self.foundation.CFBooleanGetValue(value)

    def select(self, identifier: str) -> None:
        """Selects the input source with the identifier, turning on first its input method and the source itself when the Mac has them off."""
        name = self.string(identifier)
        mode = self.find({"kTISPropertyInputSourceID": name})
        self.foundation.CFRelease(name)
        if mode is None:
            raise BuildFailed(f'This Mac has no input source "{identifier}", which a UI test selects. Add its language in the keyboard settings of the Mac.')

        method = self.find({"kTISPropertyBundleID": self.property(mode, "kTISPropertyBundleID"), "kTISPropertyInputSourceType": self.constant("kTISTypeKeyboardInputMethodModeEnabled")})
        sources = [source for source in (method, mode) if source is not None]
        if self.previous is None:
            self.previous = self.carbon.TISCopyCurrentKeyboardInputSource()
        try:
            for source in sources:
                if self.is_enabled(source):
                    continue
                if self.carbon.TISEnableInputSource(source) != MacInputSources.NO_ERROR:
                    raise BuildFailed(f'The input source "{self.text(self.property(source, "kTISPropertyInputSourceID"))}" of this Mac does not turn on for a UI test.')
                self.enabled.append(self.foundation.CFRetain(source))
            if self.carbon.TISSelectInputSource(mode) != MacInputSources.NO_ERROR:
                raise BuildFailed(f'The input source "{identifier}" of this Mac cannot be selected for a UI test.')
        finally:
            for source in sources:
                self.foundation.CFRelease(source)

    def set_preference(self, domain: str, key: str, value: bool) -> None:
        """Sets a boolean preference in the domain of the settings of an input method, such as the live conversion of the Japanese one, which the input method reads from there, keeping the value it had for `restore`."""
        if not any(changed[:2] == (domain, key) for changed in self.preferences):
            self.preferences.append((domain, key, self.copy_preference(domain, key)))
        constant = "kCFBooleanTrue" if value else "kCFBooleanFalse"
        if not self.write_preference(domain, key, ctypes.c_void_p.in_dll(self.foundation, constant).value):
            raise BuildFailed(f'The setting "{key}" of the input method "{domain}" of this Mac cannot be changed for a UI test.')

    def copy_preference(self, domain: str, key: str) -> int | None:
        """The value of a preference, retained, or None when the Mac does not hold it."""
        domain_name = self.string(domain)
        key_name = self.string(key)
        value = self.foundation.CFPreferencesCopyAppValue(key_name, domain_name)
        self.foundation.CFRelease(key_name)
        self.foundation.CFRelease(domain_name)
        return value

    def write_preference(self, domain: str, key: str, value: int | None) -> bool:
        """Writes the value of a preference, or removes it for None, and tells whether the Mac stored the change."""
        domain_name = self.string(domain)
        key_name = self.string(key)
        self.foundation.CFPreferencesSetAppValue(key_name, value, domain_name)
        stored = self.foundation.CFPreferencesAppSynchronize(domain_name)
        self.foundation.CFRelease(key_name)
        self.foundation.CFRelease(domain_name)
        return stored

    def restore(self) -> None:
        """Gives back the values that the preferences had before the run set them, selects the input source that was current before the first selection again and turns off the input sources that the run turned on, the last one first."""
        failed: list[str] = []
        for domain, key, value in reversed(self.preferences):
            if not self.write_preference(domain, key, value):
                failed.append(f'the setting "{key}" of "{domain}"')
            if value is not None:
                self.foundation.CFRelease(value)
        self.preferences = []
        if self.previous is not None:
            if self.carbon.TISSelectInputSource(self.previous) != MacInputSources.NO_ERROR:
                failed.append(f'the input source "{self.text(self.property(self.previous, "kTISPropertyInputSourceID"))}"')
            self.foundation.CFRelease(self.previous)
            self.previous = None
        for source in reversed(self.enabled):
            if self.carbon.TISDisableInputSource(source) != MacInputSources.NO_ERROR:
                failed.append(f'the input source "{self.text(self.property(source, "kTISPropertyInputSourceID"))}"')
            self.foundation.CFRelease(source)
        self.enabled = []
        if failed:
            names = ", ".join(failed)
            raise BuildFailed(f"The UI tests could not give this Mac back {names}. Set them again in the keyboard settings of the Mac.")
