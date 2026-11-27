import re
from pathlib import PurePosixPath

from haylen.core.repository import Repository
from tests.support.engine_sources import EngineSources
from tests.support.tracked_files import TrackedFiles


class SourceLayoutTest:
    """The sources sit where a reader looks for them and reach only what their layer allows: the source of a public header mirrors its folder, the engine names no folder of the samples or the templates, apps include only the public headers of the engine, and a macro only configures a dependency."""

    # The folders that hold one implementation of a capability among others, of which the build picks one by the system interface or by what the platform allows.
    VARIANTS = frozenset({"libffi", "posix", "unavailable", "windows"})
    OWN_HEADER = re.compile(r'#\s*(?:include|import)\s*"haylen/([^"]+\.(?:hpp|h))"')
    APP_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".m", ".mm")
    QUOTED_INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*"([^"]+)"', re.MULTILINE)
    DEFINE = re.compile(r"^\s*#\s*define\s+([^\s(]+)", re.MULTILINE)
    # The macros that configure a dependency before its header or its implementation, the system headers of Windows among them, and the macro that exports the functions of a native library.
    DEPENDENCY_MACROS = re.compile(r"^(?:STB\w*|STBI_\w+|MINIAUDIO_\w+|VMA_\w+|NANOSVG\w*|SDL_\w+|IMGUI_\w+|IM_\w+|WIN32_LEAN_AND_MEAN|NOMINMAX|[A-Z0-9_]+_EXPORT|\{\{MACRO\}\}_EXPORT)$")

    @staticmethod
    def misplaced_source(name: str, text: str) -> str | None:
        """The public header that a source of `engine/src` implements when the source lies outside the folder that mirrors the header, or nothing. A source implements the header of its own name that it includes first."""
        own = SourceLayoutTest.OWN_HEADER.search(text)
        source = PurePosixPath(name).relative_to("engine/src")
        if not own or PurePosixPath(own.group(1)).stem != source.stem:
            return None
        header = PurePosixPath(own.group(1)).parent
        folder = source.parent
        if folder == header or (folder.parent == header and folder.name in SourceLayoutTest.VARIANTS):
            return None
        return f"haylen/{own.group(1)}"

    @staticmethod
    def repository_folders() -> re.Pattern[str]:
        """The paths of the folders of the samples and the templates, such as `samples/tests/` or `templates/lua/`, which name the content of the repository rather than a folder of an app."""
        folders = sorted({"/".join(PurePosixPath(name).parts[:2]) for name in TrackedFiles.names("samples", "templates")})
        return re.compile(r"\b(?:" + "|".join(re.escape(folder) for folder in folders) + r")\b")

    @staticmethod
    def private_include(included: str) -> bool:
        """Whether a quoted include of an app reaches a header of the engine that is not public."""
        if included.startswith("haylen/"):
            return not any((root / included).is_file() for root in (Repository.ENGINE / "include", Repository.ENGINE / "lua" / "include"))
        return any((root / included).is_file() for root in (Repository.ENGINE / "src", Repository.ENGINE / "lua" / "src"))

    def test_the_source_of_a_public_header_mirrors_its_folder(self):
        misplaced = []
        for name, text in EngineSources.read().items():
            if name.startswith("engine/src/") and not name.endswith((".h", ".hpp")):
                header = SourceLayoutTest.misplaced_source(name, text)
                if header:
                    misplaced.append(f"{name} implements {header}")
        assert misplaced == []

    def test_the_scan_finds_a_source_outside_the_folder_of_its_header(self):
        assert SourceLayoutTest.misplaced_source("engine/src/platform/native/NativeLibraries.cpp", '#include "haylen/platform/NativeLibraries.hpp"') == "haylen/platform/NativeLibraries.hpp"
        assert SourceLayoutTest.misplaced_source("engine/src/ui/components/collections/CollectionIds.cpp", '#include "haylen/ui/CollectionIds.hpp"') == "haylen/ui/CollectionIds.hpp"
        assert SourceLayoutTest.misplaced_source("engine/src/platform/native/libffi/NativeCallback.cpp", '#include "haylen/platform/native/NativeCallback.hpp"') is None
        assert SourceLayoutTest.misplaced_source("engine/src/ui/Gui.cpp", '#include "haylen/ui/Gui.hpp"') is None
        assert SourceLayoutTest.misplaced_source("engine/src/ui/Backend.cpp", '#include "haylen/ui/Context.hpp"') is None

    def test_the_engine_names_no_folder_of_the_samples_or_the_templates(self):
        folders = SourceLayoutTest.repository_folders()
        assert folders.search("Run `samples/tests` on the headless player.")
        naming = []
        for name in TrackedFiles.names("engine"):
            content = (Repository.ROOT / name).read_bytes()
            if b"\0" in content:
                continue
            for number, line in enumerate(content.decode("utf-8").splitlines(), 1):
                found = folders.search(line)
                if found:
                    naming.append(f"{name}:{number}: {found.group(0)}")
        assert naming == []

    def test_apps_include_only_the_public_headers_of_the_engine(self):
        assert SourceLayoutTest.private_include("core/EngineState.hpp")
        assert SourceLayoutTest.private_include("haylen/core/EngineState.hpp")
        assert not SourceLayoutTest.private_include("haylen/core/Engine.hpp")
        including = []
        for name in TrackedFiles.names("samples", "templates"):
            if name.endswith(SourceLayoutTest.APP_SUFFIXES):
                text = (Repository.ROOT / name).read_text(encoding="utf-8")
                including += [f"{name}: {included}" for included in SourceLayoutTest.QUOTED_INCLUDE.findall(text) if SourceLayoutTest.private_include(included)]
        assert including == []

    def test_macros_only_configure_dependencies(self):
        defining = []
        for name in TrackedFiles.names("engine", "samples", "templates"):
            if name.endswith((*SourceLayoutTest.APP_SUFFIXES, ".in")):
                text = (Repository.ROOT / name).read_text(encoding="utf-8")
                defining += [f"{name}: {macro}" for macro in SourceLayoutTest.DEFINE.findall(text) if not SourceLayoutTest.DEPENDENCY_MACROS.match(macro)]
        assert defining == []
