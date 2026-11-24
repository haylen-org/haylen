from haylen_ui_test_case import HaylenUiTestCase


class TextInputTests(HaylenUiTestCase):
    """The fields of the test "Text fields" type through the keyboard of Windows: each key types once, Backspace deletes, a held Backspace goes on deleting while it repeats, the Microsoft Japanese IME composes text, a paste replaces the selection, and Tab moves to the next field."""

    launch_arguments = ("--test", "GUI-005")
    URL = "https://haylen.dev"
    NAME = "Island name"
    # The keyboard layouts of the tests: the US keyboard, which every test starts with, and the Microsoft Japanese IME, which composes kana from Latin letters.
    ENGLISH = "00000409"
    JAPANESE = "00000411"

    # The test focuses its first field a few frames after it shows, so the tests act once it did, as a person does who sees the field edit. Windows hands a new app the input method that the person used last, so each test starts with the US keyboard, whatever input method a test before it used.
    def setup_method(self) -> None:
        super().setup_method()
        self.wait_for_focus(TextInputTests.NAME)
        self.use_keyboard_layout(TextInputTests.ENGLISH)

    def expect_report(self, field: str, text: str) -> None:
        """Waits for the status line, which the test sets on every change of a field."""
        self.wait_for(f'The field "{field}" reported "change" with "{text}".', 10)

    def test_each_key_types_once_and_backspace_deletes(self) -> None:
        self.start_typing(TextInputTests.URL)
        self.type_keys("oi")
        self.expect_report("url", "oi")
        self.press(HaylenUiTestCase.VK_BACK)
        self.expect_report("url", "o")
        self.type_keys("la")
        self.expect_report("url", "ola")
        self.save_screenshot("windows-text-typed")

    # The key goes on deleting while it is held, until the field is empty.
    def test_holding_backspace_goes_on_deleting(self) -> None:
        self.start_typing(TextInputTests.URL)
        self.type_keys("ilhas")
        self.expect_report("url", "ilhas")
        self.hold_until(HaylenUiTestCase.VK_BACK, 'The field "url" reported "change" with "".', 10)
        self.save_screenshot("windows-backspace-held")

    def test_an_input_method_composes_the_text_of_the_field(self) -> None:
        self.start_typing(TextInputTests.NAME)
        previous = self.use_keyboard_layout(TextInputTests.JAPANESE)
        try:
            self.press(HaylenUiTestCase.VK_IME_ON)
            # The input method keeps a last "n" open for the kana that may follow it, and a second "n" closes it as "ん".
            self.type_keys("nihonn")
            self.press(HaylenUiTestCase.VK_RETURN)
            self.expect_report("text", "にほん")
            self.save_screenshot("windows-composed")
        finally:
            self.press(HaylenUiTestCase.VK_IME_OFF)
            self.restore_keyboard_layout(previous)

    def test_paste_replaces_the_selection(self) -> None:
        self.start_typing(TextInputTests.URL)
        self.type_keys("mar")
        self.expect_report("url", "mar")
        self.set_clipboard("ilha")
        self.press(ord("A"), HaylenUiTestCase.VK_CONTROL)
        self.press(ord("V"), HaylenUiTestCase.VK_CONTROL)
        self.expect_report("url", "ilha")
        self.save_screenshot("windows-pasted")

    def test_tab_moves_to_the_next_field(self) -> None:
        self.start_typing(TextInputTests.URL)
        self.type_keys("a")
        self.expect_report("url", "a")
        self.press(HaylenUiTestCase.VK_TAB)
        self.type_keys("b")
        self.expect_report("search", "b")
        self.save_screenshot("windows-next-field")
