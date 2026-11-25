import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class TestDiscoveryTest:
    """The discovery of the GoogleTest programs runs in the build tree, since some versions of CMake write the list of the tests that a program reports into the working directory of the discovery and leave it there."""

    CALL = re.compile(r"\bgtest_discover_tests\s*\(([^)]*)\)")

    def test_no_discovery_works_in_the_sources(self):
        named = []
        for name in TrackedFiles.names("*CMakeLists.txt", "*.cmake"):
            text = (Repository.ROOT / name).read_text(encoding="utf-8")
            for call in TestDiscoveryTest.CALL.finditer(text):
                if re.search(r"\bWORKING_DIRECTORY\b", call.group(1)):
                    named.append(f"{name}: {call.group(1).split()[0]}")
        assert named == []
