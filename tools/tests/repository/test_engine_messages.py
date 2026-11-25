import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class EngineMessagesTest:
    """The messages of the engine, of its Lua bindings and of the content tool speak to an app developer who never saw the code of the engine, so no text that a throw, a Lua error or a log line carries holds the path of a JSON document, the name of a constant of the code, a code of an enum or a name of the standard library."""

    # The calls whose texts reach a person: a thrown exception, a Lua error and a line of the log.
    CALL = re.compile(r"\bthrow\s+[\w:]+\s*[({]|\bluaL_error\s*\(|\bLog::(?:error|warning|info|debug)\s*\(")
    # An index of a list after a name, with a number or a value that the message fills in, the root of a JSON path, a constant of the code, a code of an enum, a name of the standard library, a method of the Java code of the engine and a library that the engine uses.
    INTERNAL = re.compile(r"\w\[(?:\d+|\{\})\]|\$\.|\bk[A-Z][A-Za-z0-9]+\b|\bCode::|\bstd::|\bdev\.haylen\.\w+\.\w+|\b(?:SDL|Dear ImGui|Varn|nlohmann)\b")
    # The texts that the audio library and the window system write for their failures, which no message of the engine repeats.
    LIBRARY_TEXT = re.compile(r"\b(?:ma_result_description|SDL_GetError)\s*\(")
    # A failure of a GPU device that describes a broken contract in words instead of throwing the rule that the call breaks.
    CONTRACT_TEXT = re.compile(r'throw std::(?:logic_error|invalid_argument|out_of_range)\(\s*(?:std::format\(\s*)?"')
    GPU_SOURCES = ("engine/src/gpu/*.cpp", "engine/src/gpu/*.hpp", "engine/src/gpu/*.mm")
    SOURCES = (
        "engine/src/*.cpp",
        "engine/src/*.hpp",
        "engine/src/*.mm",
        "engine/include/*.hpp",
        "engine/lua/src/*.cpp",
        "engine/lua/src/*.hpp",
        "engine/lua/include/*.hpp",
        "engine/tools/*.cpp",
        "engine/tools/*.hpp",
    )

    @staticmethod
    def arguments(source: str, start: int) -> tuple[list[str], int]:
        """The string literals of the call whose arguments open at the bracket before `start`, and the place where the call ends."""
        literals = []
        depth = 1
        index = start
        while index < len(source) and depth > 0:
            character = source[index]
            # An apostrophe after a digit or a letter separates the digits of a number, such as 1'000, and opens no character.
            if character == "'" and source[index - 1].isalnum():
                index += 1
                continue
            if character in "\"'":
                end = index + 1
                while end < len(source) and source[end] != character:
                    end += 2 if source[end] == "\\" else 1
                if character == '"':
                    literals.append(source[index + 1 : end])
                index = end + 1
                continue
            if character in "({":
                depth += 1
            elif character in ")}":
                depth -= 1
            index += 1
        return literals, index

    @staticmethod
    def messages(source: str) -> list[tuple[int, str]]:
        """The texts of the messages of a source file with their lines: every string literal in the arguments of a throw, a Lua error or a log call."""
        found = []
        for match in EngineMessagesTest.CALL.finditer(source):
            literals, _ = EngineMessagesTest.arguments(source, match.end())
            line = source.count("\n", 0, match.start()) + 1
            found.extend((line, literal) for literal in literals)
        return found

    def test_the_rule_finds_internals_in_a_message(self):
        source = 'throw std::invalid_argument("The key \\"slices[0]\\" is wrong.");\nluaL_error(L, "The value of $.window is wrong.");\ncore::Log::error("The limit is {}.", kMaxSize);\nthrow Error(Error::Code::CorruptHeader, "The code Code::CorruptHeader failed.");\nthrow std::runtime_error(std::string("The image ") + "could not load.");\nLog::warning("Item 2 of \\"plugins\\" is fine.");\nLog::error("The Java method \\"dev.haylen.HaylenBridge.dispatch\\" threw.");\nthrow std::runtime_error("SDL lists no display.");\n'
        assert [line for line, text in EngineMessagesTest.messages(source) if EngineMessagesTest.INTERNAL.search(text)] == [1, 2, 4, 7, 8]

    def test_no_message_of_the_engine_holds_internals(self):
        wrong = []
        for name in TrackedFiles.names(*EngineMessagesTest.SOURCES):
            for line, text in EngineMessagesTest.messages((Repository.ROOT / name).read_text(encoding="utf-8")):
                if EngineMessagesTest.INTERNAL.search(text):
                    wrong.append(f"{name}:{line}: {text}")
        assert wrong == []

    def test_no_source_of_the_engine_repeats_the_text_of_a_library(self):
        """The engine tells why the audio output did not open and why the window system refused a request with a sentence of Haylen, so no source reads the text that the audio library writes for a result or that the window system writes for a failure."""
        wrong = []
        for name in TrackedFiles.names(*EngineMessagesTest.SOURCES):
            source = (Repository.ROOT / name).read_text(encoding="utf-8")
            wrong.extend(f"{name}:{source.count(chr(10), 0, match.start()) + 1}" for match in EngineMessagesTest.LIBRARY_TEXT.finditer(source))
        assert wrong == []

    def test_the_rule_finds_the_text_of_a_library(self):
        source = 'throw std::runtime_error("The window could not open. " + std::string(SDL_GetError()));\ncore::Log::error("{}", ma_result_description(result));\nthrow std::runtime_error("The window could not open.");\n'
        assert len(EngineMessagesTest.LIBRARY_TEXT.findall(source)) == 2

    def test_the_rule_finds_a_contract_described_in_words(self):
        source = 'throw std::logic_error(std::format("The pipeline {} does not exist.", id));\nthrow ContractError(ContractError::Rule::PassOrder);\nthrow std::runtime_error("The GPU ran out of memory.");\n'
        assert len(EngineMessagesTest.CONTRACT_TEXT.findall(source)) == 1

    def test_the_gpu_devices_throw_the_rule_of_a_broken_contract(self):
        """Only a bug of the engine or of a port breaks the contract of a GPU device, so the devices throw `gpu::ContractError`, whose message asks the person to report it, and never a text that describes the objects of the device."""
        wrong = []
        for name in TrackedFiles.names(*EngineMessagesTest.GPU_SOURCES):
            source = (Repository.ROOT / name).read_text(encoding="utf-8")
            for match in EngineMessagesTest.CONTRACT_TEXT.finditer(source):
                wrong.append(f"{name}:{source.count(chr(10), 0, match.start()) + 1}")
        assert wrong == []
