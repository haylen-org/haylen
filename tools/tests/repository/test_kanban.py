import re
from dataclasses import dataclass

from haylen.core.repository import Repository
from tests.support.markdown_page import MarkdownPage


class KanbanTest:
    """The board of `KANBAN.md` agrees with its records, which are the source of truth: every active record has one cell in the column of its status and every cell a record, each header counts its cells, the cells of a column follow its order, the active records follow their IDs, every archived record carries its date, and the next ID is above every ID in the file."""

    BOARD = Repository.ROOT / "KANBAN.md"
    COLUMNS = {"TODO": "todo", "DOING": "doing", "BLOCKED": "blocked", "REVIEW": "review", "TESTING": "testing", "DONE": "done"}
    PRIORITIES = {"critical": "🔴", "high": "🟠", "medium": "🟡", "low": "🟢"}
    HEADER = re.compile(r"^(\w+) \((\d+) tasks?(?:, [^)]+)?\)$")
    CELL = re.compile(r"^\*\*\[KB-(\d+) · (.+?)\]\(#kb-(\d+)\)\*\*<br>(\S+) (\w+) · `([\w-]+)` · (\d+)/10$")
    FIELD = re.compile(r"^- \*\*([\w ]+):\*\* (.*)$")

    @dataclass(frozen=True)
    class Cell:
        number: int
        title: str
        priority: str
        type: str
        complexity: str

    @staticmethod
    def sections(text: str) -> tuple[str, str, str]:
        """The board, the active records and the archive of the file."""
        board = text.split("<!-- KANBAN:BOARD:START -->", 1)[1].split("<!-- KANBAN:BOARD:END -->", 1)[0]
        cards = text.split("<!-- KANBAN:CARDS:START -->", 1)[1].split("## Working agreements", 1)[0]
        archive = text.split("<!-- KANBAN:ARCHIVE -->", 1)[1]
        return board, cards, archive

    @staticmethod
    def records(section: str) -> dict[int, dict[str, str]]:
        """The fields of every record of a section by its number, in the order of the section."""
        found: dict[int, dict[str, str]] = {}
        for block in re.split(r"^### KB-", section, flags=re.MULTILINE)[1:]:
            number, _, body = block.partition("\n")
            fields = {}
            for line in body.splitlines():
                field = KanbanTest.FIELD.match(line)
                if field and field.group(1) not in fields:
                    fields[field.group(1)] = field.group(2)
                if line.startswith("#### "):
                    break
            found[int(number)] = fields
        return found

    @staticmethod
    def columns(board: str) -> dict[str, tuple[int, list[KanbanTest.Cell], list[str]]]:
        """The count that the header of each column shows, its cells and the cells it cannot read."""
        rows = [MarkdownPage.cells(line) for line in board.strip().splitlines() if line.startswith("|")]
        headers = rows[0]
        columns: dict[str, tuple[int, list[KanbanTest.Cell], list[str]]] = {}
        for index, header in enumerate(headers):
            heading = KanbanTest.HEADER.match(header)
            name, count = (heading.group(1), int(heading.group(2))) if heading else (header, -1)
            cells: list[KanbanTest.Cell] = []
            unreadable: list[str] = []
            for row in rows[2:]:
                text = row[index] if index < len(row) else ""
                if text == "—":
                    continue
                cell = KanbanTest.CELL.match(text)
                if not cell or cell.group(1) != cell.group(3):
                    unreadable.append(text)
                    continue
                cells.append(KanbanTest.Cell(int(cell.group(1)), cell.group(2), f"{cell.group(4)} {cell.group(5)}", cell.group(6), cell.group(7)))
            columns[name] = (count, cells, unreadable)
        return columns

    @staticmethod
    def problems(text: str) -> list[str]:
        board, cards, archive = KanbanTest.sections(text)
        records = KanbanTest.records(cards)
        archived = KanbanTest.records(archive)
        columns = KanbanTest.columns(board)
        problems = []

        if list(columns) != list(KanbanTest.COLUMNS):
            problems.append(f"The board has the columns {list(columns)}.")
        placed: dict[int, str] = {}
        for name, (count, cells, unreadable) in columns.items():
            problems += [f"The column {name} holds a cell that is not a card: {cell}" for cell in unreadable]
            if count != len(cells):
                problems.append(f"The header of the column {name} counts {count} cards and the column holds {len(cells)}.")
            for cell in cells:
                if cell.number in placed:
                    problems.append(f"The card KB-{cell.number:03d} has a cell in {placed[cell.number]} and in {name}.")
                placed[cell.number] = name
                record = records.get(cell.number)
                if record is None:
                    problems.append(f"The cell of KB-{cell.number:03d} in {name} has no active record.")
                    continue
                if record.get("Status") != KanbanTest.COLUMNS.get(name):
                    problems.append(f"The card KB-{cell.number:03d} is {record.get('Status')} and its cell is in {name}.")
                title = record.get("Title", "")
                if cell.title != title and not (cell.title.endswith("…") and title.startswith(cell.title[:-1])):
                    problems.append(f"The cell of KB-{cell.number:03d} shows another title than its record.")
                priority = record.get("Priority", "")
                if cell.priority != f"{KanbanTest.PRIORITIES.get(priority, '?')} {priority}" or cell.type != record.get("Type") or cell.complexity != record.get("Complexity"):
                    problems.append(f"The cell of KB-{cell.number:03d} shows another priority, type or complexity than its record.")
            if name == "DONE":
                finished = [(records.get(cell.number, {}).get("Finished", "unknown"), cell.number) for cell in cells]
                known = [entry for entry in finished if entry[0] != "unknown"]
                if finished != sorted(known, reverse=True) + sorted((entry for entry in finished if entry[0] == "unknown"), reverse=True):
                    problems.append("The column DONE is out of the order of the dates the cards finished and their IDs.")
            else:
                order = [(list(KanbanTest.PRIORITIES).index(records[cell.number]["Priority"]), cell.number) for cell in cells if records.get(cell.number, {}).get("Priority") in KanbanTest.PRIORITIES]
                if order != sorted(order):
                    problems.append(f"The column {name} is out of the order of priority and ID.")

        problems += [f"The active record KB-{number:03d} has no cell." for number in records if number not in placed]
        if list(records) != sorted(records):
            problems.append("The active records are out of the order of their IDs.")
        problems += [f"The archived record KB-{number:03d} has no date of its archive." for number, fields in archived.items() if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", fields.get("Archived", ""))]
        problems += [f"The card KB-{number:03d} is both active and archived." for number in records.keys() & archived.keys()]

        next_id = re.search(r"\*\*Next ID:\*\* KB-(\d+)\.", text)
        highest = max(int(number) for number in re.findall(r"^### KB-(\d+)$", text, flags=re.MULTILINE))
        if not next_id or int(next_id.group(1)) <= highest:
            problems.append(f"The next ID is not above the highest ID, KB-{highest:03d}.")
        return problems

    def test_the_board_agrees_with_its_records(self):
        assert KanbanTest.problems(KanbanTest.BOARD.read_text(encoding="utf-8")) == []

    def test_the_check_finds_a_board_that_disagrees_with_its_records(self):
        record = "### KB-{number:03d}\n\n- **Title:** {title}\n- **Status:** {status}\n- **Type:** task\n- **Priority:** {priority}\n- **Complexity:** 3\n\n#### Description\n\nUm cartão.\n\n"
        cell = "**[KB-{number:03d} · {title}](#kb-{number:03d})**<br>{label} {priority} · `task` · 3/10"
        board = "\n".join(
            [
                "<!-- KANBAN:BOARD:START -->",
                "| TODO (3 tasks) | DOING (1 task) | BLOCKED (0 tasks) | REVIEW (0 tasks) | TESTING (0 tasks) | DONE (0 tasks, 30 days) |",
                "| :--- | :--- | :--- | :--- | :--- | :--- |",
                f"| {cell.format(number=2, title='Baixo', label='🟢', priority='low')} | {cell.format(number=3, title='Feito', label='🟠', priority='high')} | — | — | — | — |",
                f"| {cell.format(number=1, title='Alto', label='🟠', priority='high')} | — | — | — | — | — |",
                "<!-- KANBAN:BOARD:END -->",
                "<!-- KANBAN:CARDS:START -->",
                record.format(number=1, title="Alto", status="todo", priority="high"),
                record.format(number=2, title="Baixo", status="todo", priority="low"),
                record.format(number=3, title="Feito", status="done", priority="high"),
                record.format(number=4, title="Sem célula", status="todo", priority="low"),
                "## Working agreements\n\n- **Next ID:** KB-004.\n\n## Archive\n\n<!-- KANBAN:ARCHIVE -->\n",
                record.format(number=5, title="Arquivado", status="todo", priority="low"),
            ]
        )
        assert KanbanTest.problems(board) == [
            "The header of the column TODO counts 3 cards and the column holds 2.",
            "The column TODO is out of the order of priority and ID.",
            "The card KB-003 is done and its cell is in DOING.",
            "The active record KB-004 has no cell.",
            "The archived record KB-005 has no date of its archive.",
            "The next ID is not above the highest ID, KB-005.",
        ]
