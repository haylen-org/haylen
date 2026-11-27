import json
import re
from pathlib import PurePosixPath

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class DependenciesTest:
    """The dependencies of the engine are pinned and kept in one place: every package that CPM downloads carries the SHA-256 hash of its archive, nlohmann/json resolves before Varn so the build holds one copy of it, every patch of a dependency is documented and keys the source cache of its package, and no sample, template or plugin of the repository brings a library, SDK or script of a third party."""

    DEPENDENCIES = Repository.ENGINE / "cmake" / "haylen-dependencies.cmake"
    PATCHES = Repository.ENGINE / "cmake" / "patches"
    PATCH_DOCS = ("docs/tools/build.md", "docs/tools/distribution.md")
    PACKAGE = re.compile(r"\bCPMAddPackage\s*\(")
    VARIABLE = re.compile(r"^\s*set\((\w+) ([0-9a-f]{64})\)$", re.MULTILINE)
    # The files of the native parts of plugins that could fetch code while they build or run.
    PLUGIN_SUFFIXES = (".cmake", ".html", ".java", ".js", ".kt", ".mjs", ".swift")
    # The contributions that bring code from outside the repository into an app.
    THIRD_PARTY_CONTRIBUTIONS = frozenset({"binaryFramework", "gradlePlugin", "mavenRepository", "swiftPackage"})
    GRADLE_DEPENDENCY = re.compile(r"""\b(?:implementation|api|compileOnly|runtimeOnly|testImplementation|androidTestImplementation|ksp|kapt|classpath)\s*\(\s*["']([^"']+)["']""")
    DOWNLOADS = re.compile(
        r"\b(?:CPMAddPackage|FetchContent_Declare|ExternalProject_Add|file\s*\(\s*DOWNLOAD)\b|\bimport\b[^;\n]*['\"]https?://|<script[^>]+src=['\"]https?://|\bimportScripts\s*\(\s*['\"]https?://"
    )

    @staticmethod
    def packages(text: str) -> list[tuple[str, str]]:
        """The name and the arguments of every package that a CMake file adds with CPM."""
        found = []
        for match in DependenciesTest.PACKAGE.finditer(text):
            depth = 1
            end = match.end()
            while depth and end < len(text):
                depth += {"(": 1, ")": -1}.get(text[end], 0)
                end += 1
            arguments = text[match.end() : end - 1]
            name = re.search(r"\bNAME\s+(\S+)", arguments)
            found.append((name.group(1) if name else "", arguments))
        return found

    @staticmethod
    def unpinned(text: str) -> list[str]:
        """The packages of a CMake file that download without the SHA-256 hash of their archive, by name. A package of a folder on the machine downloads nothing."""
        hashes = dict(DependenciesTest.VARIABLE.findall(text))
        wrong = []
        for name, arguments in DependenciesTest.packages(text):
            if "SOURCE_DIR" in arguments:
                continue
            pinned = re.search(r"\bURL_HASH\s+SHA256=(?:([0-9a-f]{64})|\$\{(\w+)\})(?=\s)", arguments + "\n")
            if not pinned or (pinned.group(2) and pinned.group(2) not in hashes) or re.search(r"\bGIT_(?:TAG|REPOSITORY)\b|\bGITHUB_REPOSITORY\b", arguments):
                wrong.append(name)
        return wrong

    def test_every_package_carries_the_hash_of_its_archive(self):
        assert DependenciesTest.unpinned("CPMAddPackage(\n  NAME a\n  URL x\n  URL_HASH SHA256=" + "a" * 64 + "\n)") == []
        assert DependenciesTest.unpinned("CPMAddPackage(\n  NAME b\n  GIT_TAG main\n  URL_HASH SHA256=" + "a" * 64 + "\n)") == ["b"]
        assert DependenciesTest.unpinned("CPMAddPackage(\n  NAME c\n  URL x\n  URL_HASH SHA256=${c_hash}\n)") == ["c"]
        assert DependenciesTest.unpinned('CPMAddPackage(NAME d URL "x" OPTIONS "A ON")') == ["d"]
        assert DependenciesTest.unpinned('CPMAddPackage(NAME haylen SOURCE_DIR "${engine}")') == []
        unpinned = []
        for name in TrackedFiles.names("*CMakeLists.txt", "*.cmake"):
            unpinned += [f"{name}: {package}" for package in DependenciesTest.unpinned((Repository.ROOT / name).read_text(encoding="utf-8"))]
        assert unpinned == []

    def test_nlohmann_json_resolves_before_varn(self):
        names = [name for name, _ in DependenciesTest.packages(DependenciesTest.DEPENDENCIES.read_text(encoding="utf-8"))]
        assert "nlohmann_json" in names and "varn" in names
        assert names.index("nlohmann_json") < names.index("varn")

    def test_every_patch_is_documented_and_keys_its_package(self):
        text = DependenciesTest.DEPENDENCIES.read_text(encoding="utf-8")
        docs = "".join((Repository.ROOT / page).read_text(encoding="utf-8") for page in DependenciesTest.PATCH_DOCS)
        patches = sorted(path.name for path in DependenciesTest.PATCHES.glob("*.patch"))
        assert patches
        assert [patch for patch in patches if f"patches/{patch}" not in text] == []
        assert [patch for patch in patches if patch not in docs] == []

        unkeyed = []
        for name, arguments in DependenciesTest.packages(text):
            listed = re.search(r"\bPATCHES\s+\$\{(\w+)_patches\}", arguments)
            if "PATCHES" not in arguments:
                continue
            prefix = listed.group(1) if listed else ""
            keyed = re.search(rf"haylen_patched_package_key\({prefix}_cache_key \$\{{{prefix}_hash\}} \$\{{{prefix}_patches\}}\)", text)
            if not listed or not keyed or f"CUSTOM_CACHE_KEY ${{{prefix}_cache_key}}" not in arguments or f"SHA256=${{{prefix}_hash}}" not in arguments:
                unkeyed.append(name)
        assert unkeyed == []

    def test_no_sample_template_or_plugin_brings_code_of_a_third_party(self):
        bringing = []
        for name in TrackedFiles.names("samples", "templates"):
            path = PurePosixPath(name)
            text = (Repository.ROOT / name).read_text(encoding="utf-8", errors="replace")
            if path.name == "plugin.json":
                types = {contribution.get("type") for contribution in json.loads(text).get("contributions", [])}
                bringing += [f"{name}: {kind}" for kind in sorted(types & DependenciesTest.THIRD_PARTY_CONTRIBUTIONS)]
            elif path.name.endswith((".gradle.kts", ".gradle")):
                bringing += [f"{name}: {coordinates}" for coordinates in DependenciesTest.GRADLE_DEPENDENCY.findall(text) if not coordinates.startswith("dev.haylen:")]
            elif "plugins" in path.parts and (path.suffix in DependenciesTest.PLUGIN_SUFFIXES or path.name == "CMakeLists.txt"):
                found = DependenciesTest.DOWNLOADS.search(text)
                if found:
                    bringing.append(f"{name}: {found.group(0)}")
        assert bringing == []
