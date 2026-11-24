from haylen.engine.mac_keyboard import MacKeyboard


class MacKeyboardTest:
    """A UI test of the Mac asks to hold a key with a line of its output that names the virtual key code and the repeats, and every other line holds nothing."""

    def test_a_line_that_asks_holds_its_key_with_its_repeats(self, monkeypatch):
        held: list[tuple[int, int]] = []
        monkeypatch.setattr(MacKeyboard, "hold", lambda code, repeats: held.append((code, repeats)))
        for line in ("Test Case '-[HaylenUITests.MacTextInputTests testHolding]' started.", "HaylenHoldKey: 51 3\r", "HaylenHoldKey: delete 3"):
            MacKeyboard.answer(line)
        assert held == [(51, 3)]
