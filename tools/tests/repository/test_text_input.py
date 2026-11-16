import re

from haylen.core.repository import Repository
from tests.support.engine_sources import EngineSources


class TextInputTest:
    """The text input of SDL reads keys as text only on Windows and Linux, where no native field edits, so keys in play on every other platform never reach an input method, such as the accent menu of macOS."""

    def test_only_the_text_input_of_windows_and_linux_starts_the_text_input_of_sdl(self):
        starting = [name for name, text in EngineSources.read().items() if re.search(r"\bSDL_StartTextInput\w*\s*\(", text)]
        assert starting == ["engine/src/platform/sdl/TextInput.cpp"]
        build = (Repository.ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        assert re.search(r'if\(HAYLEN_PLATFORM MATCHES "\^\(windows\|linux\)\$"\)\n\s*list\(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sdl/TextInput\.cpp\)', build)
        assert build.count("src/platform/sdl/TextInput.cpp") == 1
