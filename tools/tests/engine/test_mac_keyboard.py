from haylen.engine import mac_keyboard
from haylen.engine.mac_keyboard import MacKeyboard


class MacKeyboardTest:
    """A UI test of the Mac asks with a line of its output to hold a key with its virtual key code and repeats, to select an input source by its identifier or to give the Mac back its input sources, and every other line asks for nothing. The input sources go back only once a test selected one."""

    class Sources:
        """Input sources of the Mac that record what the run asks of them."""

        def __init__(self, calls: list[str]) -> None:
            self.calls = calls

        def select(self, identifier: str) -> None:
            self.calls.append(f"select {identifier}")

        def restore(self) -> None:
            self.calls.append("restore")

    def test_a_line_that_asks_holds_its_key_with_its_repeats(self, monkeypatch):
        held: list[tuple[int, int]] = []
        monkeypatch.setattr(MacKeyboard, "hold", lambda code, repeats: held.append((code, repeats)))
        keyboard = MacKeyboard()
        for line in ("Test Case '-[HaylenUITests.MacTextInputTests testHolding]' started.", "HaylenHoldKey: 51 3\r", "HaylenHoldKey: delete 3"):
            keyboard.answer(line)
        assert held == [(51, 3)]

    def test_a_line_that_asks_selects_an_input_source_until_the_mac_gets_its_sources_back(self, monkeypatch):
        calls: list[str] = []
        monkeypatch.setattr(mac_keyboard, "MacInputSources", lambda: MacKeyboardTest.Sources(calls))
        keyboard = MacKeyboard()
        keyboard.restore()
        for line in ("HaylenInputSource: com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese\r", "Test Case started.", "HaylenRestoreInputSources"):
            keyboard.answer(line)
        assert calls == ["select com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese", "restore"]
