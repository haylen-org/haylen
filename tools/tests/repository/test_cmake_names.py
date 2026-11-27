import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class CMakeNameTest:
    """CMake targets, projects, install components and the files that targets write use dashes, in the CMake files of the repository and in the CMake text that the tooling writes from its Python."""

    # The first argument of these commands names a target or a project, on the line of the command or on the next one, also in the CMake text inside the strings of the tooling, the word after `COMPONENT` names an install component and the value of `OUTPUT_NAME` names the file of a target.
    DEFINITION = re.compile(r"^[\s'\"]*(?:add_library|add_executable|add_custom_target|haylen_add_app|project)\s*\(\s*([^\s)]*)")
    NAMED = re.compile(r"\b(?:COMPONENT|OUTPUT_NAME)\s+\"?([^\s)\"]+)")

    @staticmethod
    def underscored(text: str) -> list[str]:
        """The names of targets, projects, install components and target files with an underscore in a CMake text."""
        lines = [line for line in text.splitlines() if not line.lstrip().startswith("#")]
        found = []
        for index, line in enumerate(lines):
            names = [match.group(1) for match in CMakeNameTest.NAMED.finditer(line)]
            definition = CMakeNameTest.DEFINITION.match(line)
            if definition and definition.group(1):
                names.append(definition.group(1))
            elif definition and index + 1 < len(lines):
                names.append(lines[index + 1].strip().strip("'\"").split()[0] if lines[index + 1].strip() else "")
            for named in names:
                # A variable names nothing by itself, and the tokens of the plugin template that become identifiers count as underscores.
                bare = re.sub(r"\$\{[^}]*\}", "", named)
                if "_" in bare or "{{SYMBOL}}" in bare or "{{MACRO}}" in bare:
                    found.append(named)
        return found

    def test_targets_projects_and_components_use_dashes(self):
        names = TrackedFiles.names("*CMakeLists.txt", "*.cmake", "*.cmake.in", "tools/haylen/*.py")
        underscored = []
        for name in names:
            underscored += [f"{name}: {found}" for found in CMakeNameTest.underscored((Repository.ROOT / name).read_text(encoding="utf-8"))]
        assert underscored == []

    def test_the_scan_reads_the_name_on_the_next_line_and_the_names_of_files(self):
        text = "add_library(\n  native_demo SHARED Demo.cpp)\nset_target_properties(main PROPERTIES OUTPUT_NAME native_tool)\ninstall(TARGETS main COMPONENT haylen_sdk)\nadd_executable(${HAYLEN_APP} Main.cpp)\n# add_library(commented_out)\n"
        assert CMakeNameTest.underscored(text) == ["native_demo", "native_tool", "haylen_sdk"]
