import io
from unittest import mock

from haylen.development.key_reader import KeyReader


class KeyReaderTest:
    def test_the_reader_names_the_keys_that_edit(self):
        reader = KeyReader(io.StringIO())
        assert [reader.name(character) for character in ("a", "\r", "\t", "\x7f", "\x15", "\x02")] == ["a", "Enter", "Tab", "Backspace", "ClearLine", ""]

        with mock.patch.object(reader, "waiting", return_value=True), mock.patch.object(reader, "character", side_effect=["[", "A", "[", "3", "~"]):
            assert reader.name("\x1b") == "Up"
            assert reader.name("\x1b") == "Delete"
        with mock.patch.object(reader, "waiting", return_value=False):
            assert reader.name("\x1b") == "Escape"
