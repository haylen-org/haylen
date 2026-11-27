import re
from pathlib import PurePosixPath

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class RepositoryFilesTest:
    """The repository holds only what belongs in it: every font sits next to the license file that its license requires, the sibling repositories are named only where they are described, no private or temporary file is tracked, the layers of the app scaffolds share no file, and the scaffolds carry the format settings of the repository."""

    FONT_SUFFIXES = (".otf", ".ttf", ".woff", ".woff2")
    # The URL of each sibling repository with the files that describe it or pin it.
    SIBLINGS = {
        "github.com/haylen-org/haylen-runner": ("AGENTS.md", "KANBAN.md"),
        "github.com/haylen-org/haylen-plugins": ("AGENTS.md", "KANBAN.md"),
        "github.com/haylen-org/haylen-shader-compiler": ("AGENTS.md", "KANBAN.md", "docs/graphics/shaders.md", "docs/tools/build.md", "engine/cmake/haylen-shader-compiler.json"),
    }
    RUNNER = re.compile(r"\bhaylen-runner\b|\bHaylen Runner\b")
    PRIVATE = re.compile(
        r"\.(?:cer|crt|der|jks|key|keystore|log|mobileprovision|p12|p8|pem|pfx|provisionprofile)$"
        r"|(?:^|/)(?:\.env(?:\.[^/]*)?|local\.properties|\.DS_Store|Thumbs\.db)$"
        r"|(?:^|/)(?:build|\.cache|\.gradle|\.tools|\.venv|__pycache__|DerivedData|node_modules|xcuserdata)/"
    )
    LAYERS = ("templates/lua", "templates/cpp")
    FORMATTED_SCAFFOLDS = ("templates/common", "templates/plugin")

    @staticmethod
    def text(name: str) -> str | None:
        content = (Repository.ROOT / name).read_bytes()
        return None if b"\0" in content else content.decode("utf-8", errors="replace")

    def test_every_font_sits_next_to_its_license(self):
        folders = {PurePosixPath(name).parent for name in TrackedFiles.names() if name.lower().endswith(RepositoryFilesTest.FONT_SUFFIXES)}
        assert folders
        licensed = {PurePosixPath(name).parent for name in TrackedFiles.names() if re.search(r"(?:^|-)license\.(?:txt|md)$", PurePosixPath(name).name.lower())}
        assert sorted(str(folder) for folder in folders - licensed) == []

    def test_the_sibling_repositories_are_named_only_where_they_are_described(self):
        naming = []
        own = PurePosixPath(__file__).name
        for name in TrackedFiles.names():
            text = RepositoryFilesTest.text(name)
            if text is None or name.endswith(own):
                continue
            for url, places in RepositoryFilesTest.SIBLINGS.items():
                if url in text and name not in places:
                    naming.append(f"{name}: {url}")
            if RepositoryFilesTest.RUNNER.search(text) and name not in ("AGENTS.md", "KANBAN.md"):
                naming.append(f"{name}: the runner")
        assert naming == []

    def test_no_private_or_temporary_file_is_tracked(self):
        assert RepositoryFilesTest.PRIVATE.search("platform/android/keystore/upload.jks")
        assert RepositoryFilesTest.PRIVATE.search("samples/tests/build/apps/log.txt")
        assert RepositoryFilesTest.PRIVATE.search("tools/__pycache__/terminal.cpython-315.pyc")
        assert not RepositoryFilesTest.PRIVATE.search("tools/haylen/platforms/android/template/app/build.gradle.kts")
        assert [name for name in TrackedFiles.names() if RepositoryFilesTest.PRIVATE.search(name)] == []

    def test_the_layers_of_the_app_scaffolds_share_no_file(self):
        common = {PurePosixPath(name).relative_to("templates/common") for name in TrackedFiles.names("templates/common")}
        assert common
        repeated = []
        for layer in RepositoryFilesTest.LAYERS:
            files = {PurePosixPath(name).relative_to(layer) for name in TrackedFiles.names(layer)}
            assert files
            repeated += [f"{layer}/{path}" for path in sorted(files & common)]
        assert repeated == []

    def test_the_scaffolds_carry_the_format_settings_of_the_repository(self):
        settings = (Repository.ROOT / ".clang-format").read_text(encoding="utf-8")
        differing = [scaffold for scaffold in RepositoryFilesTest.FORMATTED_SCAFFOLDS if (Repository.ROOT / scaffold / ".clang-format").read_text(encoding="utf-8") != settings]
        assert differing == []
