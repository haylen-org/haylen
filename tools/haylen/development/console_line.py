import shutil

from haylen.terminal import Terminal


class ConsoleLine:
    """The line that the developer types in the developer console of a running app: its text with the cursor, the lines typed before in this run, which the arrows bring back, and completions from the commands of the app."""

    PROMPT = "> "
    HISTORY = 100
    EMPTY_HINT = '"help" lists the commands, and Escape or Enter goes back to the keys'

    def __init__(self) -> None:
        self.text = ""
        self.cursor = 0
        self.history: list[str] = []
        # How many lines back the arrows went, and the line being typed before they went back.
        self.recalled = 0
        self.draft = ""
        # The commands of the app as `{name, usage, help, arguments}`, which it sends as the console opens and after every line.
        self.commands: list[dict] = []

    def edit(self, key: str) -> str | None:
        """Applies a key that edits the line, and returns the line once Enter confirms it."""
        if key == "Enter":
            return self.take()
        if key == "Backspace" and self.cursor > 0:
            self.text = self.text[: self.cursor - 1] + self.text[self.cursor :]
            self.cursor -= 1
        elif key == "Delete":
            self.text = self.text[: self.cursor] + self.text[self.cursor + 1 :]
        elif key == "Left":
            self.cursor = max(0, self.cursor - 1)
        elif key == "Right":
            self.cursor = min(len(self.text), self.cursor + 1)
        elif key == "Home":
            self.cursor = 0
        elif key == "End":
            self.cursor = len(self.text)
        elif key == "ClearLine":
            self.replace("")
        elif key in ("Up", "Down"):
            self.recall(1 if key == "Up" else -1)
        elif len(key) == 1:
            self.text = self.text[: self.cursor] + key + self.text[self.cursor :]
            self.cursor += 1
        return None

    def replace(self, text: str) -> None:
        self.text = text
        self.cursor = len(text)

    def take(self) -> str:
        line = self.text.strip()
        if line and (not self.history or self.history[-1] != line):
            self.history = [*self.history, line][-ConsoleLine.HISTORY :]
        self.recalled = 0
        self.replace("")
        return line

    def recall(self, steps: int) -> None:
        recalled = min(max(self.recalled + steps, 0), len(self.history))
        if recalled == self.recalled:
            return
        if self.recalled == 0:
            self.draft = self.text
        self.recalled = recalled
        self.replace(self.history[-recalled] if recalled else self.draft)

    def complete(self) -> list[str]:
        """Completes the word before the cursor: the name of a command as the first word, and "true" or "false" for a boolean argument. One candidate completes the word, and several complete what they start with and come back to be listed."""
        before = self.text[: self.cursor]
        words = before.split()
        typing = bool(before) and not before[-1].isspace()
        word = words[-1] if typing else ""
        position = len(words) - 1 if typing else len(words)

        if position == 0:
            candidates = [command["name"] for command in self.commands if command["name"].startswith(word)]
        else:
            command = next((command for command in self.commands if command["name"] == words[0]), None)
            arguments = command["arguments"] if command else []
            boolean = position - 1 < len(arguments) and arguments[position - 1].rstrip("?") == "boolean"
            candidates = [value for value in ("false", "true") if boolean and value.startswith(word)]
        if not candidates:
            return []

        completed = candidates[0] + " " if len(candidates) == 1 else ConsoleLine.shared_start(candidates)
        self.text = before[: len(before) - len(word)] + completed + self.text[self.cursor :]
        self.cursor = len(before) - len(word) + len(completed)
        return candidates if len(candidates) > 1 else []

    @staticmethod
    def shared_start(words: list[str]) -> str:
        """The longest start that every word shares."""
        shared = words[0]
        for word in words[1:]:
            length = 0
            while length < min(len(shared), len(word)) and shared[length] == word[length]:
                length += 1
            shared = shared[:length]
        return shared

    def render(self, color: bool) -> str:
        """Draws the prompt and the part of the line around the cursor that fits the terminal, and puts the cursor in its place. An empty line on a terminal with color shows in a dim color how to list the commands and how to leave, when it fits."""
        room = max(shutil.get_terminal_size().columns - len(ConsoleLine.PROMPT) - 1, 1)
        prompt = Terminal.paint(ConsoleLine.PROMPT, color, Terminal.EXPRESSION)
        if not self.text and color and len(ConsoleLine.EMPTY_HINT) <= room:
            return f"{prompt}{Terminal.render(ConsoleLine.EMPTY_HINT, color, Terminal.LABEL)}\x1b[{Terminal.visible_width(Terminal.render(ConsoleLine.EMPTY_HINT, color))}D"

        start = max(0, self.cursor - room)
        shown = self.text[start : start + room]
        back = len(shown) - (self.cursor - start)
        return prompt + shown + (f"\x1b[{back}D" if back > 0 else "")
