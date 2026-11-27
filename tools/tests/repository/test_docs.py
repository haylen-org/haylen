import re
import unicodedata
from pathlib import Path

from haylen.core.repository import Repository
from tests.support.markdown_page import MarkdownPage
from tests.support.tracked_files import TrackedFiles


class DocsTest:
    """The documentation is one tree that a reader walks from the main page: every page starts with its title, which alone carries an emoji, names the language of its code blocks and keeps each paragraph on one line, every row of a table has as many cells as its header, every relative link reaches its page and heading, the indexes list their pages in tables of name, description and link that never mix guides with reference pages, and no long sentence repeats in another page. The docs mirror the code: every Lua module has its reference page in both indexes, every folder of 2D and 3D has its guide, every guide shows code, every folder of Lua bindings has its tests, and no semicolon joins sentences."""

    DOCS = Repository.ROOT / "docs"
    HEADER = "| Name | Description | Link |"
    # The blocks that hold reference pages: the Lua API, and the manifests with the commands.
    REFERENCE_BLOCKS = frozenset({"lua-api", "reference"})
    PRELOAD = re.compile(r'\bBinding::preload\(\s*\w+\s*,\s*"(haylen(?:\.\w+)?)"')
    # A semicolon of prose, outside code and the entities of HTML such as `&lt;`.
    SEMICOLON = re.compile(r"(?<!&[a-z]{2})(?<!&[a-z]{3})(?<!&[a-z]{4});")

    @staticmethod
    def page_of(module: str) -> str:
        """The reference page of a Lua module, named in dashes, such as `hot-reload.md` for `haylen.hotReload`."""
        name = module.removeprefix("haylen.") if module != "haylen" else "haylen"
        return re.sub(r"(?<!^)([A-Z])", r"-\1", name).lower() + ".md"

    @staticmethod
    def listed(index: Path, block: Path) -> set[str]:
        """The pages of a block that the tables of an index link."""
        found = set()
        for _, rows in MarkdownPage(index).tables():
            for row in rows:
                found |= {(index.parent / target).resolve().name for target in MarkdownPage.row_links(row) if (index.parent / target).resolve().parent == block.resolve()}
        return found

    @staticmethod
    def semicolons(line: str) -> list[str]:
        """The sentences of a line of prose that a semicolon joins, outside its code spans."""
        prose = re.sub(r"`[^`]*`", "", line)
        return [sentence.strip() for sentence in re.split(r"(?<=[.!?])\s+", prose) if DocsTest.SEMICOLON.search(sentence)]

    @staticmethod
    def pages(*patterns: str) -> list[Path]:
        return [Repository.ROOT / name for name in TrackedFiles.names(*(patterns or ("*.md",)))]

    @staticmethod
    def shown(path: Path) -> str:
        return path.relative_to(Repository.ROOT).as_posix()

    @staticmethod
    def indexes() -> list[Path]:
        return [DocsTest.DOCS / "README.md", *sorted(DocsTest.DOCS.glob("*/README.md"))]

    def test_every_page_starts_with_a_title_that_starts_with_an_emoji(self):
        # The main page opens with the logo, an issue template opens with the front matter whose name GitHub shows, and the board keeps the fixed title of its format.
        wrong = []
        for path in DocsTest.pages():
            name = DocsTest.shown(path)
            if name in ("README.md", "KANBAN.md") or name.startswith(".github/ISSUE_TEMPLATE/"):
                continue
            title = path.read_text(encoding="utf-8").split("\n", 1)[0]
            if not title.startswith("# ") or unicodedata.category(title[2:3]) != "So":
                wrong.append(f"{name}: {title}")
        assert wrong == []

    def test_no_section_heading_carries_an_emoji(self):
        # Only the title carries an emoji, so the anchors of the sections stay plain. The cloud tracker keeps the fixed section titles of its format, whose emoji show the state of each part at a glance.
        wrong = []
        for path in DocsTest.pages():
            if DocsTest.shown(path) == "CLOUD.md":
                continue
            for number, line in MarkdownPage(path).prose():
                if not re.match(r"^#{2,6} ", line):
                    continue
                if any(unicodedata.category(character) == "So" for character in line):
                    wrong.append(f"{DocsTest.shown(path)}:{number}: {line}")
        assert wrong == []

    def test_every_code_block_names_its_language(self):
        wrong = []
        for path in DocsTest.pages():
            inside = False
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                if not line.lstrip().startswith("```"):
                    continue
                if not inside and line.strip() == "```":
                    wrong.append(f"{DocsTest.shown(path)}:{number}")
                inside = not inside
        assert wrong == []

    def test_every_paragraph_is_one_line(self):
        wrong = []
        for path in DocsTest.pages():
            previous = False
            for number, line in MarkdownPage(path).prose():
                text = bool(line.strip()) and not re.match(r"^\s*([|#>*+-]|\d+\.|<)", line)
                if text and previous:
                    wrong.append(f"{DocsTest.shown(path)}:{number}")
                previous = text
        assert wrong == []

    def test_relative_links_reach_their_pages_and_headings(self):
        anchors: dict[Path, set[str]] = {}
        wrong = []
        for path in DocsTest.pages():
            for number, _, target in MarkdownPage(path).links():
                if re.match(r"^[a-z]+:", target):
                    continue
                file, _, anchor = target.partition("#")
                destination = (path.parent / file).resolve() if file else path.resolve()
                where = f"{DocsTest.shown(path)}:{number}: {target}"
                if not destination.exists():
                    wrong.append(f"{where} names no file")
                    continue
                if not anchor or destination.suffix != ".md":
                    continue
                if destination not in anchors:
                    anchors[destination] = MarkdownPage(destination).anchors()
                if anchor not in anchors[destination]:
                    wrong.append(f"{where} names no heading")
        assert wrong == []

    def test_the_main_page_links_the_documentation_only_through_its_index(self):
        targets = [target for _, _, target in MarkdownPage(Repository.ROOT / "README.md").links() if target.startswith(("docs/", "./docs/"))]
        assert targets == ["docs/README.md"]

    def test_every_page_of_the_documentation_is_reached_from_the_index_through_its_block(self):
        index = MarkdownPage(DocsTest.DOCS / "README.md")
        blocks = {(DocsTest.DOCS / target).resolve() for _, rows in index.tables() for row in rows for target in MarkdownPage.row_links(row)}
        assert blocks == {path.resolve() for path in DocsTest.DOCS.glob("*/README.md")}

        reached = set()
        for block in DocsTest.DOCS.glob("*/README.md"):
            reached |= {(block.parent / target).resolve() for _, rows in MarkdownPage(block).tables() for row in rows for target in MarkdownPage.row_links(row)}
        pages = {path.resolve() for path in DocsTest.DOCS.rglob("*.md") if path.name != "README.md"}
        assert sorted(DocsTest.shown(path) for path in pages - reached) == []
        assert sorted(path.relative_to(DocsTest.DOCS).as_posix() for path in DocsTest.DOCS.rglob("*.md") if path.parent.parent != DocsTest.DOCS and path.parent != DocsTest.DOCS) == []

    def test_index_tables_list_name_description_and_link_in_alphabetical_order(self):
        wrong = []
        for path in DocsTest.indexes():
            name = DocsTest.shown(path)
            for section, rows in MarkdownPage(path).tables():
                if rows[0] != DocsTest.HEADER or len(rows) < 3:
                    wrong.append(f'{name}, {section}: the table needs the header "{DocsTest.HEADER}" and rows')
                    continue
                names = []
                for row in rows[2:]:
                    cells = [cell.strip() for cell in row.strip("|").split(" | ")]
                    if len(cells) != 3 or not re.fullmatch(r"\[`[^`]+`\]\([^)\s]+\)", cells[2]):
                        wrong.append(f"{name}, {section}: {row}")
                        continue
                    names.append(re.sub(r"[^\w .]", "", cells[0].replace("`", "")).strip().casefold())
                if names != sorted(names):
                    wrong.append(f"{name}, {section}: the rows are out of alphabetical order")
        assert wrong == []

    def test_every_row_of_a_table_has_as_many_cells_as_its_header(self):
        wrong = [f"{DocsTest.shown(path)}, {section}: {row}" for path in DocsTest.pages() for section, row in MarkdownPage(path).uneven_rows()]
        assert wrong == []

    def test_a_row_with_a_cell_too_few_or_too_many_is_found(self, tmp_path: Path):
        page = tmp_path / "page.md"
        rows = ["| Device | Safe area | Cutout |", "| --- | --- | --- |", "| Phone | 44 | Notch |", "| Tablet | 24 |", "| Watch | 8 | None | Extra |", "| Code | `a \\| b` | Escaped |"]
        page.write_text("\n".join(["# 📖 Page", "", "## Devices", "", *rows, ""]), encoding="utf-8")
        assert list(MarkdownPage(page).uneven_rows()) == [("Devices", "| Tablet | 24 |"), ("Devices", "| Watch | 8 | None | Extra |")]

    def test_no_table_of_an_index_mixes_guides_with_reference_pages(self):
        references = {(DocsTest.DOCS / block).resolve() for block in DocsTest.REFERENCE_BLOCKS}
        wrong = []
        for path in DocsTest.indexes():
            for section, rows in MarkdownPage(path).tables():
                kinds = {(path.parent / target).resolve().parent in references for row in rows for target in MarkdownPage.row_links(row)}
                if len(kinds) > 1:
                    wrong.append(f"{DocsTest.shown(path)}, {section}")
        assert wrong == []

    def test_no_long_sentence_repeats_in_another_page_of_the_documentation(self):
        # A subject lives in one page, and the other pages link to it. Reference pages share the sentences that describe the same rule of every module, such as how options are checked.
        seen: dict[str, str] = {}
        wrong = []
        for path in [Repository.ROOT / "README.md", *DocsTest.pages("docs/*.md")]:
            name = DocsTest.shown(path)
            for _, line in MarkdownPage(path).prose():
                if line.startswith(("|", "#")):
                    continue
                for sentence in re.split(r"(?<=\.)\s+(?=[A-Z])", line.strip()):
                    first = seen.setdefault(sentence, name) if len(sentence) >= 100 else name
                    if first != name and not (first.startswith("docs/lua-api/") and name.startswith("docs/lua-api/")):
                        wrong.append(f"{first} and {name}: {sentence}")
        assert wrong == []

    def test_every_lua_module_has_its_reference_page_listed_in_both_indexes(self):
        # The page of a module takes its name in dashes, such as `hot-reload.md` for `haylen.hotReload`, and the module `haylen` itself is `haylen.md`.
        modules = set()
        for name in TrackedFiles.names("engine/lua"):
            if name.endswith(".cpp"):
                modules.update(DocsTest.PRELOAD.findall((Repository.ROOT / name).read_text(encoding="utf-8")))
        assert modules
        pages = {DocsTest.page_of(module) for module in modules}
        reference = DocsTest.DOCS / "lua-api"
        assert sorted(pages) == sorted(path.name for path in reference.glob("*.md") if path.name != "README.md")
        in_reference = DocsTest.listed(reference / "README.md", reference)
        in_blocks = set().union(*(DocsTest.listed(index, reference) for index in DocsTest.DOCS.glob("*/README.md") if index.parent != reference))
        assert sorted(pages - in_reference) == []
        assert sorted(pages - in_blocks) == []

    def test_every_folder_of_2d_and_3d_has_its_guide(self):
        missing = []
        for dimension in ("2d", "3d"):
            for folder in sorted((Repository.ENGINE / "include" / "haylen" / dimension).iterdir()):
                if folder.is_dir() and not (DocsTest.DOCS / dimension / f"{folder.name}.md").is_file():
                    missing.append(f"docs/{dimension}/{folder.name}.md")
        assert missing == []

    def test_every_guide_shows_code(self):
        # A guide explains its capability with an example, while the indexes and the reference blocks list pages and names.
        wrong = []
        for path in DocsTest.pages("docs/*.md"):
            if path.name == "README.md" or path.parent.name in DocsTest.REFERENCE_BLOCKS:
                continue
            if not any(line.lstrip().startswith("```") for line in path.read_text(encoding="utf-8").splitlines()):
                wrong.append(DocsTest.shown(path))
        assert wrong == []

    def test_every_folder_of_lua_bindings_has_its_tests(self):
        folders = {Path(name).parent.relative_to("engine/lua/src") for name in TrackedFiles.names("engine/lua/src") if name.endswith("Binding.cpp")}
        assert folders
        assert sorted(str(folder) for folder in folders if not (Repository.ENGINE / "lua" / "tests" / folder).is_dir()) == []

    def test_no_semicolon_joins_sentences(self):
        assert DocsTest.semicolons("Plugins load in order; the app waits. Use `a; b` in code and &lt;kind&gt; in a heading.") == ["Plugins load in order; the app waits."]
        wrong = []
        for path in DocsTest.pages():
            if DocsTest.shown(path) == "KANBAN.md":
                continue
            wrong += [f"{DocsTest.shown(path)}:{number}: {text}" for number, line in MarkdownPage(path).prose() for text in DocsTest.semicolons(line)]
        assert wrong == []
