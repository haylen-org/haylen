from haylen.terminal import Terminal


class TerminalTest:
    def test_a_message_breaks_between_words_as_they_show_and_never_inside_a_reserved_expression(self):
        message = 'Run "haylen sdk build" once, then `build/sdk` holds it.'

        assert Terminal.wrap(message, 20, color=True) == ['Run "haylen sdk build"', "once, then `build/sdk`", "holds it."]
        assert Terminal.wrap(message, 20, color=False) == ["Run", '"haylen sdk build"', "once, then `build/sdk`", "holds it."]
        assert Terminal.wrap("A wordthatislongerthantheline fits.", 10, color=False) == ["A", "wordthatislongerthantheline", "fits."]
