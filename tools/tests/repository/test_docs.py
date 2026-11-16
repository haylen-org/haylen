import re
import unicodedata
from pathlib import Path

from haylen.core.repository import Repository
from tests.support.markdown_page import MarkdownPage
from tests.support.tracked_files import TrackedFiles


class DocsTest:
    """The documentation is one tree that a reader walks from the main page: every page starts with its title, names the language of its code blocks and keeps each paragraph on one line, every relative link reaches its page and heading, the indexes list their pages in tables of name, description and link, and no long sentence repeats in another page."""

    DOCS = Repository.ROOT / "docs"
    HEADER = "| Name | Description | Link |"

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
        # The main page opens with the logo, and an issue template opens with the front matter whose name GitHub shows.
        wrong = []
        for path in DocsTest.pages():
            name = DocsTest.shown(path)
            if name == "README.md" or name.startswith(".github/ISSUE_TEMPLATE/"):
                continue
            title = path.read_text(encoding="utf-8").split("\n", 1)[0]
            if not title.startswith("# ") or unicodedata.category(title[2:3]) != "So":
                wrong.append(f"{name}: {title}")
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

    def test_no_table_of_an_index_mixes_guides_with_the_lua_reference(self):
        reference = (DocsTest.DOCS / "lua-api").resolve()
        wrong = []
        for path in DocsTest.indexes():
            for section, rows in MarkdownPage(path).tables():
                kinds = {(path.parent / target).resolve().parent == reference for row in rows for target in MarkdownPage.row_links(row)}
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
