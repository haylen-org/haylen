import ctypes
import os
import struct
import subprocess
import time
import zlib
from ctypes import wintypes
from pathlib import Path

import comtypes.client


class HaylenUiTestCase:
    """The base of the UI tests of a Haylen app on Windows. The command "haylen test --ui <app> --target windows" builds the app and names its executable in `HAYLEN_UI_APP`, and each test starts it again with the launch arguments of its class. The GUIs of the app expose their components through UI Automation, each element named by the text it shows, and the tests press the mouse and the keys through `SendInput`, which reach the window in front as the input of a person does."""

    TIMEOUT = 60.0
    POLL = 0.25
    # The time before the first repeat of a held key and between two repeats, the defaults of the keyboard settings of Windows.
    FIRST_REPEAT_SECONDS = 0.5
    REPEAT_SECONDS = 0.033

    # The virtual keys that the tests press, as Windows names them.
    VK_BACK = 0x08
    VK_TAB = 0x09
    VK_RETURN = 0x0D
    VK_CONTROL = 0x11
    VK_IME_ON = 0x16
    VK_IME_OFF = 0x1A

    INPUT_MOUSE = 0
    INPUT_KEYBOARD = 1
    KEYEVENTF_KEYUP = 0x0002
    MOUSEEVENTF_LEFTDOWN = 0x0002
    MOUSEEVENTF_LEFTUP = 0x0004
    CF_UNICODETEXT = 13
    GMEM_MOVEABLE = 0x0002
    WM_INPUTLANGCHANGEREQUEST = 0x0050
    # The process sees the screen in physical pixels, as UI Automation reports the bounds of elements, so a click lands where an element is at any scale of the display.
    DPI_AWARENESS_PER_MONITOR_V2 = -4

    launch_arguments: tuple[str, ...] = ()

    class MouseInput(ctypes.Structure):
        _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG), ("mouseData", wintypes.DWORD), ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]

    class KeyboardInput(ctypes.Structure):
        _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD), ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]

    # The record of one event of "SendInput", whose union of a mouse event and a key event names the nested types, which the body of a nested class does not see, so their fields follow the classes.
    class Event(ctypes.Union):
        pass

    Event._fields_ = [("mi", MouseInput), ("ki", KeyboardInput)]

    class Input(ctypes.Structure):
        pass

    Input._fields_ = [("type", wintypes.DWORD), ("event", Event)]

    class BitmapInfoHeader(ctypes.Structure):
        _fields_ = [
            ("biSize", wintypes.DWORD),
            ("biWidth", wintypes.LONG),
            ("biHeight", wintypes.LONG),
            ("biPlanes", wintypes.WORD),
            ("biBitCount", wintypes.WORD),
            ("biCompression", wintypes.DWORD),
            ("biSizeImage", wintypes.DWORD),
            ("biXPelsPerMeter", wintypes.LONG),
            ("biYPelsPerMeter", wintypes.LONG),
            ("biClrUsed", wintypes.DWORD),
            ("biClrImportant", wintypes.DWORD),
        ]

    def setup_method(self) -> None:
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
        self.declare()
        self.user32.SetProcessDpiAwarenessContext(HaylenUiTestCase.DPI_AWARENESS_PER_MONITOR_V2)
        comtypes.client.GetModule("UIAutomationCore.dll")
        from comtypes.gen import UIAutomationClient

        self.uia = UIAutomationClient
        self.automation = comtypes.client.CreateObject(UIAutomationClient.CUIAutomation, interface=UIAutomationClient.IUIAutomation)
        executable = os.environ.get("HAYLEN_UI_APP")
        assert executable, 'Run the UI tests with "haylen test --ui <app> --target windows", which names the app they drive.'
        arguments = ["--", *self.launch_arguments] if self.launch_arguments else []
        self.process = subprocess.Popen([executable, *arguments], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.window = self.wait_for_window()

    def teardown_method(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait(HaylenUiTestCase.TIMEOUT)

    def declare(self) -> None:
        """Gives the functions of Windows that the tests call the types of their arguments, so handles keep their 64 bits."""
        user32, kernel32, gdi32 = self.user32, self.kernel32, self.gdi32
        user32.SendInput.argtypes = (wintypes.UINT, ctypes.c_void_p, ctypes.c_int)
        user32.SetCursorPos.argtypes = (ctypes.c_int, ctypes.c_int)
        user32.SetProcessDpiAwarenessContext.argtypes = (ctypes.c_ssize_t,)
        user32.OpenClipboard.argtypes = (wintypes.HWND,)
        user32.SetClipboardData.argtypes = (wintypes.UINT, wintypes.HANDLE)
        user32.SetClipboardData.restype = wintypes.HANDLE
        user32.LoadKeyboardLayoutW.argtypes = (wintypes.LPCWSTR, wintypes.UINT)
        user32.LoadKeyboardLayoutW.restype = ctypes.c_void_p
        user32.GetKeyboardLayout.argtypes = (wintypes.DWORD,)
        user32.GetKeyboardLayout.restype = ctypes.c_void_p
        user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.c_void_p)
        user32.GetForegroundWindow.restype = wintypes.HWND
        user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, ctypes.c_void_p)
        user32.GetDC.argtypes = (wintypes.HWND,)
        user32.GetDC.restype = wintypes.HDC
        user32.ReleaseDC.argtypes = (wintypes.HWND, wintypes.HDC)
        kernel32.GlobalAlloc.argtypes = (wintypes.UINT, ctypes.c_size_t)
        kernel32.GlobalAlloc.restype = wintypes.HGLOBAL
        kernel32.GlobalLock.argtypes = (wintypes.HGLOBAL,)
        kernel32.GlobalLock.restype = ctypes.c_void_p
        kernel32.GlobalUnlock.argtypes = (wintypes.HGLOBAL,)
        gdi32.CreateCompatibleDC.argtypes = (wintypes.HDC,)
        gdi32.CreateCompatibleDC.restype = wintypes.HDC
        gdi32.CreateCompatibleBitmap.argtypes = (wintypes.HDC, ctypes.c_int, ctypes.c_int)
        gdi32.CreateCompatibleBitmap.restype = wintypes.HBITMAP
        gdi32.SelectObject.argtypes = (wintypes.HDC, wintypes.HGDIOBJ)
        gdi32.SelectObject.restype = wintypes.HGDIOBJ
        gdi32.BitBlt.argtypes = (wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int, wintypes.DWORD)
        gdi32.GetDIBits.argtypes = (wintypes.HDC, wintypes.HBITMAP, wintypes.UINT, wintypes.UINT, ctypes.c_void_p, ctypes.c_void_p, wintypes.UINT)
        gdi32.DeleteObject.argtypes = (wintypes.HGDIOBJ,)
        gdi32.DeleteDC.argtypes = (wintypes.HDC,)

    def wait_until(self, message: str, condition, timeout: float = TIMEOUT) -> None:
        """Checks the condition four times a second until it holds, and fails with the message when it never does."""
        deadline = time.monotonic() + timeout
        while not condition():
            assert time.monotonic() < deadline, message
            assert self.process.poll() is None, f"The app ended with the status {self.process.returncode} while the test waited. {message}"
            time.sleep(HaylenUiTestCase.POLL)

    def wait_for_window(self):
        """The element of the window of the app, once UI Automation lists it."""
        condition = self.automation.CreatePropertyCondition(self.uia.UIA_ProcessIdPropertyId, self.process.pid)
        deadline = time.monotonic() + HaylenUiTestCase.TIMEOUT
        while True:
            window = self.automation.GetRootElement().FindFirst(self.uia.TreeScope_Children, condition)
            if window:
                return window
            assert self.process.poll() is None, f"The app ended with the status {self.process.returncode} before its window showed."
            assert time.monotonic() < deadline, f"The app shows no window after {HaylenUiTestCase.TIMEOUT:g} seconds."
            time.sleep(HaylenUiTestCase.POLL)

    def element(self, name: str):
        """The first element of the window of the app whose name is the text, or None."""
        condition = self.automation.CreatePropertyCondition(self.uia.UIA_NamePropertyId, name)
        found = self.window.FindFirst(self.uia.TreeScope_Descendants, condition)
        return found if found else None

    def wait_for(self, name: str, timeout: float = TIMEOUT):
        """Waits until the window of the app shows an element with the name, and returns it."""
        deadline = time.monotonic() + timeout
        while True:
            element = self.element(name)
            if element is not None:
                return element
            assert self.process.poll() is None, f'The app ended with the status {self.process.returncode} while the test waited for "{name}".'
            assert time.monotonic() < deadline, f'The app shows no "{name}" after {timeout:g} seconds. It shows {self.describe()}.'
            time.sleep(HaylenUiTestCase.POLL)

    def describe(self) -> str:
        """The names of the elements of the window, so a wait that fails tells what the app showed."""
        names = []
        elements = self.window.FindAll(self.uia.TreeScope_Descendants, self.automation.CreateTrueCondition())
        for index in range(elements.Length):
            name = elements.GetElement(index).CurrentName
            if name:
                names.append(f'"{name}"')
        return ", ".join(names) or "nothing"

    def send(self, kind: int, event: ctypes.Structure) -> None:
        field = "mi" if kind == HaylenUiTestCase.INPUT_MOUSE else "ki"
        record = HaylenUiTestCase.Input(type=kind, event=HaylenUiTestCase.Event(**{field: event}))
        sent = self.user32.SendInput(1, ctypes.byref(record), ctypes.sizeof(record))
        assert sent == 1, f"Windows takes no input from the test, error {ctypes.get_last_error()}. The UI tests need the desktop of a person who is signed in, unlocked and in front."

    def click(self, name: str) -> None:
        """Clicks the middle of the element with the name, as the mouse of a person does."""
        bounds = self.wait_for(name).CurrentBoundingRectangle
        self.user32.SetCursorPos((bounds.left + bounds.right) // 2, (bounds.top + bounds.bottom) // 2)
        for flag in (HaylenUiTestCase.MOUSEEVENTF_LEFTDOWN, HaylenUiTestCase.MOUSEEVENTF_LEFTUP):
            self.send(HaylenUiTestCase.INPUT_MOUSE, HaylenUiTestCase.MouseInput(dwFlags=flag))

    def wait_for_focus(self, name: str) -> None:
        """Waits until the element with the name takes the keyboard focus, such as the field that a screen focuses when it opens, a few frames after it shows."""
        element = self.wait_for(name)
        self.wait_until(f'The element "{name}" does not take the keyboard focus.', lambda: bool(element.CurrentHasKeyboardFocus), 10)

    def start_typing(self, name: str) -> None:
        """Clicks the field with the name and waits until it takes the keyboard focus, which the keys that the test types then reach."""
        self.click(name)
        self.wait_for_focus(name)

    def in_front(self) -> bool:
        """Whether the window in front of the desktop belongs to the app, which the keys of the tests reach."""
        process = wintypes.DWORD()
        self.user32.GetWindowThreadProcessId(self.user32.GetForegroundWindow(), ctypes.byref(process))
        return process.value == self.process.pid

    def key(self, code: int, down: bool) -> None:
        assert self.in_front(), (
            "The window of the app is not in front of the desktop, so the keys of the test would reach another window. Close every other window of the desktop of the user who runs the tests, such as the first sign-in screens of Windows."
        )
        flags = 0 if down else HaylenUiTestCase.KEYEVENTF_KEYUP
        self.send(HaylenUiTestCase.INPUT_KEYBOARD, HaylenUiTestCase.KeyboardInput(wVk=code, dwFlags=flags))

    def press(self, code: int, *modifiers: int) -> None:
        """Presses a virtual key once, with the modifier keys held around it."""
        for modifier in modifiers:
            self.key(modifier, True)
        self.key(code, True)
        self.key(code, False)
        for modifier in reversed(modifiers):
            self.key(modifier, False)

    def type_keys(self, text: str) -> None:
        """Types the letters and digits of the text with their keys, one press each, as a keyboard does without Shift."""
        for character in text:
            assert character.isascii() and character.isalnum(), f'The text "{text}" holds "{character}", which has no key of its own.'
            self.press(ord(character.upper()))

    def hold_until(self, code: int, name: str, timeout: float = TIMEOUT) -> None:
        """Holds a virtual key down until the app shows an element with the name, as a person holds a key while they watch what it does, and lets it go. A keyboard sends a held key as a key down again for each repeat, which Windows marks as a repeat since the key is already down, after the delay and at the rate of the keyboard settings."""
        deadline = time.monotonic() + timeout
        self.key(code, True)
        time.sleep(HaylenUiTestCase.FIRST_REPEAT_SECONDS)
        try:
            while self.element(name) is None:
                assert time.monotonic() < deadline, f'The app shows no "{name}" after the key was held for {timeout:g} seconds. It shows {self.describe()}.'
                self.key(code, True)
                time.sleep(HaylenUiTestCase.REPEAT_SECONDS)
        finally:
            self.key(code, False)

    def set_clipboard(self, text: str) -> None:
        """Puts the text on the clipboard of Windows, as a copy in another app does."""
        data = (text + "\0").encode("utf-16-le")
        memory = self.kernel32.GlobalAlloc(HaylenUiTestCase.GMEM_MOVEABLE, len(data))
        target = self.kernel32.GlobalLock(memory)
        ctypes.memmove(target, data, len(data))
        self.kernel32.GlobalUnlock(memory)
        assert self.user32.OpenClipboard(None), f"The clipboard does not open, error {ctypes.get_last_error()}."
        try:
            self.user32.EmptyClipboard()
            assert self.user32.SetClipboardData(HaylenUiTestCase.CF_UNICODETEXT, memory), f"The clipboard takes no text, error {ctypes.get_last_error()}."
        finally:
            self.user32.CloseClipboard()

    def window_handle(self) -> int:
        return self.window.CurrentNativeWindowHandle

    def use_keyboard_layout(self, layout: str) -> int:
        """Switches the input of the window of the app to the keyboard layout or input method, such as "00000411" for the Microsoft Japanese IME, as the language bar does, and returns the one it had."""
        handle = self.window_handle()
        thread = self.user32.GetWindowThreadProcessId(handle, None)
        previous = self.user32.GetKeyboardLayout(thread)
        loaded = self.user32.LoadKeyboardLayoutW(layout, 0)
        assert loaded, f'Windows has no keyboard layout "{layout}". Add its language in Settings > Time & language > Language & region.'
        self.user32.PostMessageW(handle, HaylenUiTestCase.WM_INPUTLANGCHANGEREQUEST, 0, loaded)
        self.wait_until(f'The window of the app does not take the keyboard layout "{layout}".', lambda: self.user32.GetKeyboardLayout(thread) == loaded, 10)
        return previous

    def restore_keyboard_layout(self, layout: int) -> None:
        """Gives the window of the app back the keyboard layout it had, and waits until it takes it, since Windows keeps the last layout of a user for the next app."""
        handle = self.window_handle()
        thread = self.user32.GetWindowThreadProcessId(handle, None)
        self.user32.PostMessageW(handle, HaylenUiTestCase.WM_INPUTLANGCHANGEREQUEST, 0, layout)
        self.wait_until("The window of the app does not take its keyboard layout back.", lambda: self.user32.GetKeyboardLayout(thread) == layout, 10)

    def save_screenshot(self, name: str) -> None:
        """Saves a picture of the window of the app, never the whole screen, as a PNG named after the text in the folder of the screenshots of the run."""
        bounds = self.window.CurrentBoundingRectangle
        width, height = bounds.right - bounds.left, bounds.bottom - bounds.top
        screen = self.user32.GetDC(None)
        memory = self.gdi32.CreateCompatibleDC(screen)
        bitmap = self.gdi32.CreateCompatibleBitmap(screen, width, height)
        try:
            self.gdi32.SelectObject(memory, bitmap)
            self.gdi32.BitBlt(memory, 0, 0, width, height, screen, bounds.left, bounds.top, 0x00CC0020)
            header = HaylenUiTestCase.BitmapInfoHeader(biSize=ctypes.sizeof(HaylenUiTestCase.BitmapInfoHeader), biWidth=width, biHeight=-height, biPlanes=1, biBitCount=32)
            pixels = ctypes.create_string_buffer(width * height * 4)
            self.gdi32.GetDIBits(memory, bitmap, 0, height, pixels, ctypes.byref(header), 0)
        finally:
            self.gdi32.DeleteObject(bitmap)
            self.gdi32.DeleteDC(memory)
            self.user32.ReleaseDC(None, screen)
        folder = Path(os.environ["HAYLEN_UI_OUTPUT"])
        folder.mkdir(parents=True, exist_ok=True)
        (folder / f"{name}.png").write_bytes(HaylenUiTestCase.png(pixels.raw, width, height))

    @staticmethod
    def png(bgra: bytes, width: int, height: int) -> bytes:
        """Encodes pixels of blue, green, red and an unused byte as an RGB PNG."""
        rows = bytearray()
        for y in range(height):
            row = bgra[y * width * 4 : (y + 1) * width * 4]
            rgb = bytearray(width * 3)
            rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
            rows += b"\0" + rgb
        header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
        chunks = [HaylenUiTestCase.chunk(b"IHDR", header), HaylenUiTestCase.chunk(b"IDAT", zlib.compress(bytes(rows))), HaylenUiTestCase.chunk(b"IEND", b"")]
        return b"\x89PNG\r\n\x1a\n" + b"".join(chunks)

    @staticmethod
    def chunk(kind: bytes, data: bytes) -> bytes:
        """A chunk of a PNG file: its length, its kind, its data and the checksum of the kind and the data."""
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
