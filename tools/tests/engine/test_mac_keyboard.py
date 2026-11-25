from haylen.engine import mac_keyboard
from haylen.engine.mac_keyboard import MacKeyboard


class MacKeyboardTest:
    """A UI test of the Mac asks with a line of its output to hold a key with its virtual key code and repeats, to press keys one after the other, to select an input source by its identifier, to set a preference of an input method or to give the Mac back its input sources, and every other line asks for nothing. The input sources go back only once a test changed them, and the file of the run tells the test the input source that the run selected."""

    class Sources:
        """Input sources of the Mac that record what the run asks of them."""

        def __init__(self, calls: list[str]) -> None:
            self.calls = calls

        def select(self, identifier: str) -> None:
            self.calls.append(f"select {identifier}")

        def set_preference(self, domain: str, key: str, value: bool) -> None:
            self.calls.append(f"set {domain} {key} {value}")

        def restore(self) -> None:
            self.calls.append("restore")

    def test_a_line_that_asks_holds_its_key_with_its_repeats(self, monkeypatch, tmp_path):
        held: list[tuple[int, int]] = []
        monkeypatch.setattr(MacKeyboard, "hold", lambda code, repeats: held.append((code, repeats)))
        keyboard = MacKeyboard(tmp_path / "input-source")
        for line in ("Test Case '-[HaylenUITests.MacTextInputTests testHolding]' started.", "HaylenHoldKey: 51 3\r", "HaylenHoldKey: delete 3"):
            keyboard.answer(line)
        assert held == [(51, 3)]

    def test_a_line_that_asks_presses_its_keys_one_after_the_other(self, monkeypatch, tmp_path):
        posted: list[list[tuple[int, bool, bool, float]]] = []
        monkeypatch.setattr(MacKeyboard, "post", lambda presses: posted.append(presses))
        keyboard = MacKeyboard(tmp_path / "input-source")
        for line in ("HaylenPressKeys: 45 34\r", "HaylenPressKeys: n i", "HaylenPressKeys:"):
            keyboard.answer(line)
        pause = MacKeyboard.TYPING_SECONDS
        assert posted == [[(45, True, False, pause), (45, False, False, pause), (34, True, False, pause), (34, False, False, pause)]]

    def test_a_line_that_asks_selects_an_input_source_until_the_mac_gets_its_sources_back(self, monkeypatch, tmp_path):
        calls: list[str] = []
        monkeypatch.setattr(mac_keyboard, "MacInputSources", lambda: MacKeyboardTest.Sources(calls))
        keyboard = MacKeyboard(tmp_path / "input-source")
        status = tmp_path / "input-source"
        keyboard.restore()
        assert status.read_text(encoding="utf-8") == ""
        keyboard.answer("HaylenInputSource: com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese\r")
        assert status.read_text(encoding="utf-8") == "com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese"
        for line in ("Test Case started.", "HaylenRestoreInputSources"):
            keyboard.answer(line)
        assert calls == ["select com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese", "restore"]
        assert status.read_text(encoding="utf-8") == ""

    def test_a_line_that_asks_sets_a_preference_of_an_input_method_before_its_input_source(self, monkeypatch, tmp_path):
        calls: list[str] = []
        monkeypatch.setattr(mac_keyboard, "MacInputSources", lambda: MacKeyboardTest.Sources(calls))
        keyboard = MacKeyboard(tmp_path / "input-source")
        for line in (
            "HaylenInputMethodPreference: com.apple.inputmethod.Kotoeri JIMPrefLiveConversionKey false\r",
            "HaylenInputMethodPreference: com.apple.inputmethod.Kotoeri JIMPrefLiveConversionKey no",
            "HaylenInputSource: com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese",
            "HaylenRestoreInputSources",
        ):
            keyboard.answer(line)
        assert calls == [
            "set com.apple.inputmethod.Kotoeri JIMPrefLiveConversionKey False",
            "select com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese",
            "restore",
        ]
