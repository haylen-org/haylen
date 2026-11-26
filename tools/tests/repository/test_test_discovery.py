import re

from haylen.core.repository import Repository
from tests.support.engine_sources import EngineSources
from tests.support.namespace_scope import NamespaceScope
from tests.support.tracked_files import TrackedFiles


class TestDiscoveryTest:
    """The discovery of the GoogleTest programs runs in the build tree, since some versions of CMake write the list of the tests that a program reports into the working directory of the discovery and leave it there. A test never takes the printed value of its parameter as its name, which holds the raw bytes of an object on some compilers and changes with every build: a value-parameterized suite names its runs with a name generator, and a typed suite keeps the numbers that GoogleTest gives its types, since the versions of CMake before 4.2 cannot read the name of a typed suite that a generator names."""

    CALL = re.compile(r"\bgtest_discover_tests\s*\(([^)]*)\)")
    SUITE = re.compile(r"\b(INSTANTIATE_TEST_SUITE_P|INSTANTIATE_TYPED_TEST_SUITE_P|TYPED_TEST_SUITE)\s*\(")
    # The count of the arguments of each macro that makes the runs of a suite with a name generator.
    NAMED_ARGUMENTS = {"INSTANTIATE_TEST_SUITE_P": 4, "INSTANTIATE_TYPED_TEST_SUITE_P": 4, "TYPED_TEST_SUITE": 3}
    KEPT_NAMES = ("NO_PRETTY_TYPES", "NO_PRETTY_VALUES")

    @staticmethod
    def arguments(code: str, start: int) -> list[str]:
        """The arguments of the macro call whose parenthesis opens before `start`, split at the commas outside every bracket and template argument list."""
        arguments = []
        current = []
        depth = 0
        angles = 0
        for index in range(start, len(code)):
            character = code[index]
            if character in "([{":
                depth += 1
            elif character in ")]}":
                if depth == 0:
                    arguments.append("".join(current).strip())
                    return arguments
                depth -= 1
            elif character == "<" and depth == 0 and re.search(r"\w\s*$", code[:index]):
                angles += 1
            elif character == ">" and depth == 0 and angles > 0:
                angles -= 1
            elif character == "," and depth == 0 and angles == 0:
                arguments.append("".join(current).strip())
                current = []
                continue
            current.append(character)
        raise ValueError("A macro call of a test suite never closes.")

    @staticmethod
    def misnamed_suites(text: str) -> list[str]:
        """The suites of a source whose runs take names that change between builds or that CMake cannot read: a value-parameterized suite without a name generator and a typed suite with one, by their macro and their fixture."""
        code = NamespaceScope.stripped(text)
        misnamed = []
        for call in TestDiscoveryTest.SUITE.finditer(code):
            macro = call.group(1)
            arguments = TestDiscoveryTest.arguments(code, call.end())
            named = len(arguments) >= TestDiscoveryTest.NAMED_ARGUMENTS[macro]
            if named != (macro == "INSTANTIATE_TEST_SUITE_P"):
                fixture = arguments[1] if macro.startswith("INSTANTIATE") else arguments[0]
                misnamed.append(f"{macro} of {fixture}")
        return misnamed

    def test_no_discovery_works_in_the_sources(self):
        named = []
        for name in TrackedFiles.names("*CMakeLists.txt", "*.cmake"):
            text = (Repository.ROOT / name).read_text(encoding="utf-8")
            for call in TestDiscoveryTest.CALL.finditer(text):
                if re.search(r"\bWORKING_DIRECTORY\b", call.group(1)):
                    named.append(f"{name}: {call.group(1).split()[0]}")
        assert named == []

    def test_every_discovery_keeps_the_names_of_googletest(self):
        printed = []
        for name in TrackedFiles.names("*CMakeLists.txt", "*.cmake"):
            text = (Repository.ROOT / name).read_text(encoding="utf-8")
            for call in TestDiscoveryTest.CALL.finditer(text):
                missing = [option for option in TestDiscoveryTest.KEPT_NAMES if not re.search(rf"\b{option}\b", call.group(1))]
                if missing:
                    printed.append(f"{name}: {call.group(1).split()[0]} lacks {', '.join(missing)}")
        assert printed == []

    def test_every_parameterized_suite_names_its_runs_for_every_version_of_cmake(self):
        misnamed = []
        for name, text in EngineSources.read().items():
            misnamed += [f"{name}: {suite}" for suite in TestDiscoveryTest.misnamed_suites(text)]
        assert misnamed == []

    def test_the_scan_finds_misnamed_suites(self):
        source = """
            // INSTANTIATE_TEST_SUITE_P(Commented, CommentTest, ::testing::Values(1));
            INSTANTIATE_TEST_SUITE_P(Callbacks, ErrorTest, ::testing::Values(Code{"a, b", R"(f(x, y))"}, Code{"c", "d"}));
            INSTANTIATE_TEST_SUITE_P(Sources, PackageTest, ::testing::Values("directory", "memory"), PackageTest::nameSource);
            INSTANTIATE_TEST_SUITE_P(Apis, DrawTest, ::testing::ValuesIn(apis()), [](const ::testing::TestParamInfo<Api>& info) { return name(info.param, 1); });
            TYPED_TEST_SUITE(GridTest, ::testing::Types<HashGrid, QuadTree>);
            TYPED_TEST_SUITE(TreeTest, ::testing::Types<HashGrid, QuadTree>, TreeNames);
            INSTANTIATE_TYPED_TEST_SUITE_P(Trees, TreeSuite, Trees);
            INSTANTIATE_TYPED_TEST_SUITE_P(Trees, NamedTreeSuite, Trees, TreeNames);
        """
        assert TestDiscoveryTest.misnamed_suites(source) == [
            "INSTANTIATE_TEST_SUITE_P of ErrorTest",
            "TYPED_TEST_SUITE of TreeTest",
            "INSTANTIATE_TYPED_TEST_SUITE_P of NamedTreeSuite",
        ]
