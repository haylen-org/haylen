import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class CMakeNameTest:
    """CMake targets, projects and install components use dashes, in the CMake files of the repository and in the CMake text that the tooling writes from its Python."""

    # The first argument of these commands names a target or a project, also in the CMake text inside the strings of the tooling, and the word after `COMPONENT` names an install component.
    DEFINITION = re.compile(r"^[\s'\"]*(?:add_library|add_executable|add_custom_target|haylen_add_app|project)\s*\(\s*([^\s)]+)")
    COMPONENT = re.compile(r"\bCOMPONENT\s+([^\s)]+)")

    def test_targets_projects_and_components_use_dashes(self):
        names = TrackedFiles.names("*CMakeLists.txt", "*.cmake", "*.cmake.in", "tools/haylen/*.py")
        underscored = []
        for name in names:
            for line in (Repository.ROOT / name).read_text(encoding="utf-8").splitlines():
                if line.lstrip().startswith("#"):
                    continue
                for match in [*CMakeNameTest.DEFINITION.finditer(line), *CMakeNameTest.COMPONENT.finditer(line)]:
                    # A variable names nothing by itself, and the tokens of the plugin template that become identifiers count as underscores.
                    named = re.sub(r"\$\{[^}]*\}", "", match.group(1))
                    if "_" in named or "{{SYMBOL}}" in named or "{{MACRO}}" in named:
                        underscored.append(f"{name}: {match.group(1)}")
        assert underscored == []
