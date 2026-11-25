import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class CMakeModuleTest:
    """Every CMake file that discovers GoogleTest tests includes the module GoogleTest itself, since CMake 3.28, the oldest that the engine builds with, keeps the script of the discovery in the scope of the directory that included the module, and a discovery in another directory finds no script and adds no test."""

    DISCOVERY = re.compile(r"^\s*gtest_discover_tests\s*\(", re.MULTILINE)
    MODULE = re.compile(r"^\s*include\s*\(\s*GoogleTest\s*\)", re.MULTILINE)

    def test_each_file_that_discovers_tests_includes_the_module(self):
        missing = []
        for name in TrackedFiles.names("*CMakeLists.txt", "*.cmake"):
            text = (Repository.ROOT / name).read_text(encoding="utf-8")
            if CMakeModuleTest.DISCOVERY.search(text) and not CMakeModuleTest.MODULE.search(text):
                missing.append(name)
        assert missing == []
