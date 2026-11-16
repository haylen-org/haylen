import collections
import re
from collections.abc import Iterator
from pathlib import Path


class MarkdownPage:
    """A Markdown page of the repository, read the way GitHub shows it: its prose outside code blocks and front matter, the anchors of its headings, its links and its tables."""

    LINK = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)\s]+)\)")

    def __init__(self, path: Path) -> None:
        self.path = path

    def prose(self) -> Iterator[tuple[int, str]]:
        """Yields the numbered lines of the page outside its code blocks and the front matter that GitHub reads from an issue template."""
        inside = False
        lines = self.path.read_text(encoding="utf-8").splitlines()
        start = lines.index("---", 1) + 1 if lines and lines[0] == "---" else 0
        for number, line in enumerate(lines[start:], start + 1):
            if line.lstrip().startswith("```"):
                inside = not inside
                continue
            if not inside:
                yield number, line

    def anchors(self) -> set[str]:
        """The anchors of the headings of the page, made the way GitHub makes them."""
        found: set[str] = set()
        seen: collections.Counter[str] = collections.Counter()
        for _, line in self.prose():
            heading = re.match(r"^#{1,6} (.+)$", line)
            if not heading:
                continue
            text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", heading.group(1)).strip().lower()
            slug = re.sub(r"[^\w\- ]", "", text).replace(" ", "-")
            found.add(slug if seen[slug] == 0 else f"{slug}-{seen[slug]}")
            seen[slug] += 1
        return found

    def links(self) -> Iterator[tuple[int, str, str]]:
        """Yields the number, text and target of every link of the page outside code."""
        for number, line in self.prose():
            for match in MarkdownPage.LINK.finditer(re.sub(r"`[^`]*`(?!\])", "", line)):
                yield number, match.group(1), match.group(2)

    @staticmethod
    def row_links(row: str) -> list[str]:
        return [match.group(2) for match in MarkdownPage.LINK.finditer(row)]

    def tables(self) -> Iterator[tuple[str, list[str]]]:
        """Yields every table of the page with the section it sits in, as its lines."""
        section = ""
        rows: list[str] = []
        for _, line in [*self.prose(), (0, "")]:
            if line.startswith("|"):
                rows.append(line)
                continue
            if rows:
                yield section, rows
                rows = []
            if line.startswith("## "):
                section = line[3:]
