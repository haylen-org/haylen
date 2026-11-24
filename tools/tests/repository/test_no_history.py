import re
from pathlib import Path

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class NoHistoryTest:
    """Every file describes only what exists now, so no text tells of a rename, an earlier name, an earlier design or a change, which git holds. The words of a rule, such as "no backward-compatibility shims", and of behavior that happens at run time, such as "a node that is no longer in the tree", pass."""

    # The wording that tells of the history of the repository in English, which every file is written in.
    ENGLISH = (
        r"\bformerly\b",
        r"\bpreviously (?:called|named|known as)\b",
        r"\bused to be\b",
        r"\b(?:was|were|has been|have been|got) renamed\b",
        r"\brenamed from\b",
        r"\b(?:now|no longer) (?:called|named)\b",
        r"\b(?:the|its|their) (?:old|former|earlier|previous) (?:name|names|spelling|api|apis|type name|module name)\b",
        r"\bfor (?:backwards?|legacy) compatibility\b",
        r"\bcompatibility alias(?:es)?\b",
        r"`[^`\n]+`[^.`\n]{0,20}\b(?:no|without an?) alias\b",
        r"`[^`\n]+` (?:became|was replaced by) `[^`\n]+`",
    )
    # The wording that tells of history in Portuguese, which only the plan of the owner is written in.
    PORTUGUESE = (
        r"\bvir(?:ou|aram)\b",
        r"\b(?:foi|foram) renomead[oa]s?\b",
        r"\brenome(?:ou|aram)\b",
        r"\brenomead[oa]s? (?:para|de)\b",
        r"\bsem alias\b",
        r"\bnomes? antigos?\b",
        r"\bantes (?:era|eram|se chamava|se chamavam)\b",
        r"\bnão (?:é|são) mais\b",
        r"\bpass(?:ou|aram) a se chamar\b",
        r"\bdeix(?:ou|aram) de\b",
    )
    PORTUGUESE_FILES = ("KANBAN.md",)
    # The patches of dependencies hold the text of their projects.
    SKIPPED = ("engine/cmake/patches/",)

    @staticmethod
    def compile(patterns: tuple[str, ...]) -> re.Pattern[str]:
        return re.compile("|".join(f"(?:{pattern})" for pattern in patterns), re.IGNORECASE)

    @staticmethod
    def text(name: str) -> str | None:
        """The text of a file, or nothing for a binary file."""
        content = (Repository.ROOT / name).read_bytes()
        if b"\0" in content:
            return None
        try:
            return content.decode("utf-8")
        except UnicodeDecodeError:
            return None

    @staticmethod
    def found(name: str, wording: re.Pattern[str]) -> list[str]:
        text = NoHistoryTest.text(name)
        if text is None:
            return []
        return [f"{name}:{number}: {match.group(0)}" for number, line in enumerate(text.splitlines(), 1) for match in wording.finditer(line)]

    def test_no_file_records_history(self):
        own = Path(__file__).resolve().relative_to(Repository.ROOT).as_posix()
        english = NoHistoryTest.compile(NoHistoryTest.ENGLISH)
        portuguese = NoHistoryTest.compile(NoHistoryTest.PORTUGUESE)
        found = []
        for name in TrackedFiles.names():
            if name == own or name.startswith(NoHistoryTest.SKIPPED):
                continue
            found += NoHistoryTest.found(name, english)
            if name in NoHistoryTest.PORTUGUESE_FILES:
                found += NoHistoryTest.found(name, portuguese)
        assert found == []

    def test_the_wording_tells_history_from_the_present(self):
        english = NoHistoryTest.compile(NoHistoryTest.ENGLISH)
        portuguese = NoHistoryTest.compile(NoHistoryTest.PORTUGUESE)
        history = (
            "The type was renamed to `Gauge`.",
            "The function `ui.gauge`, formerly `ui.meter`, draws a gauge.",
            "The component `gauge`, with no alias, draws a gauge.",
            "The type `Meter` became `Gauge`.",
            "The option is kept for backward compatibility.",
            "Its old name still works.",
        )
        present = (
            "No shortcuts, dead or obsolete code, legacy compatibility paths or backward-compatibility shims.",
            "A 3D type that arrives with the name of a 2D type renames both in the same change, with no alias.",
            "An element that is no longer there ignores it.",
            "A renamed or copied file stores nothing new.",
            "Every write replaces the old file.",
            "Enums keep their names, so old names keep their meaning.",
            "public boolean requestFocus(int direction, Rect previouslyFocusedRect)",
        )
        history_pt = (
            "- [x] O `ui::Meter` virou `ui::Gauge`, sem alias.",
            "Os arquivos foram renomeados com traço.",
            "A resposta não é mais copiada a cada `await`.",
        )
        present_pt = ("- [ ] Renomear o componente com o nome comum da área.", "Um arquivo temporário renomeado no sucesso.")
        assert [line for line in history if not english.search(line)] == []
        assert [line for line in present if english.search(line)] == []
        assert [line for line in history_pt if not portuguese.search(line)] == []
        assert [line for line in present_pt if portuguese.search(line)] == []
