import re


class NamespaceScope:
    """Reads the declarations that a C, C++ or Objective-C++ source makes at namespace scope, outside every class and function, and names the ones that the code rules forbid there: anonymous namespaces, functions and variables."""

    DIRECTIVE = re.compile(r"^\s*#\s*(\w*)")
    OBJECTIVE_C_SECTION = re.compile(r"@(?:interface|implementation|protocol)\s+\w[^;{\n]*|@end\b")
    ATTRIBUTE = re.compile(r"\[\[.*?\]\]|\b__attribute__\s*\(\(.*?\)\)|\b__declspec\s*\([^)]*\)|\balignas\s*\([^)]*\)", re.DOTALL)
    TEMPLATE_HEAD = re.compile(r"^template\s*<")
    NAMESPACE = re.compile(r"^(?:inline\s+)?namespace\b\s*([\w:]*)")
    LINKAGE = re.compile(r'^extern\s*"C"')
    MACRO_CALL = re.compile(r"^[A-Z][A-Z0-9_]*\s*\(")
    ALLOWED_HEADS = re.compile(r"^(?:using|typedef|static_assert|concept|template\s+(?:class|struct)|extern\s+template)\b|[-+@]")
    REQUIRES_CLAUSE = re.compile(r"^requires\s*(?:requires\s*(?:\([^()]*\))?\s*\{\}|\([^()]*\)|[\w:]+\s*<[^;{}()]*?>)\s*")
    REQUIRES_EXPRESSION = re.compile(r"\brequires\s*(?:\([^()]*\))?\s*$")
    MACRO_WORD = re.compile(r"\b[A-Z][A-Z0-9]*_[A-Z0-9_]*\b(?:\s*\([^()]*\))?")
    QUALIFIER = re.compile(r"((?:[A-Za-z_]\w*(?:\s*<[^;{}()]*?>)?\s*::\s*)+)$")
    CLASS_HEAD = re.compile(r"^(?:class|struct|union|enum)\b")
    OPERATOR = re.compile(r"\boperator\s*(?:\(\s*\)|\[\s*\]|(?:new|delete)\s*(?:\[\s*\])?|\"\"\s*\w+|[^\s(\w]+|\w[\w\s:<>*&]*?(?=\())")
    DECLARATOR = re.compile(r"(~?[A-Za-z_]\w*(?:\s*<[^;{}()]*?>)?(?:\s*::\s*~?[A-Za-z_]\w*(?:\s*<[^;{}()]*?>)?)*)\s*(?:\[[^\]]*\]\s*)*$")
    # Functions whose names the platform or the language dictates: the entry points of programs and of JNI, and the replacements of the global allocation functions.
    ENTRY_POINTS = re.compile(r"^(?:main|wmain|WinMain|wWinMain|JNI_OnLoad|JNI_OnUnload|Java_\w+|operator\s*(?:new|delete)(?:\s*\[\s*\])?)$")
    BLOCK_ENDS_STATEMENT = re.compile(r"[\w~@\[}]")

    @staticmethod
    def stripped(text: str) -> str:
        """The code of a source without its comments, the contents of its literals and its preprocessor lines, with each Objective-C section opened and closed as a statement, so only the code that declares remains."""
        code = []
        index = 0
        length = len(text)
        while index < length:
            character = text[index]
            following = text[index + 1] if index + 1 < length else ""
            if character == "/" and following == "/":
                end = text.find("\n", index)
                index = length if end < 0 else end
                continue
            if character == "/" and following == "*":
                end = text.find("*/", index + 2)
                code.append(" ")
                index = length if end < 0 else end + 2
                continue
            if character == '"':
                index = NamespaceScope.skip_string(text, index, code)
                continue
            if character == "'" and not NamespaceScope.is_digit_separator(text, index):
                end = index + 1
                while end < length and text[end] != "'":
                    end += 2 if text[end] == "\\" else 1
                code.append("' '")
                index = end + 1
                continue
            code.append(character)
            index += 1
        lines = iter("".join(code).replace("\\\n", " ").splitlines())
        without_preprocessor, _ = NamespaceScope.branch(lines)
        return NamespaceScope.OBJECTIVE_C_SECTION.sub(";", without_preprocessor)

    @staticmethod
    def branch(lines) -> tuple[str, str | None]:
        """The code of one branch of a conditional of the preprocessor, or of the whole source, without its directives, and the directive that ends it."""
        kept = []
        for line in lines:
            directive = NamespaceScope.DIRECTIVE.match(line)
            if not directive:
                kept.append(line)
                continue
            word = directive.group(1)
            if word in ("if", "ifdef", "ifndef"):
                kept.append(NamespaceScope.conditional(lines))
            elif word in ("else", "elif", "elifdef", "elifndef", "endif"):
                return "\n".join(kept), word
        return "\n".join(kept), None

    @staticmethod
    def conditional(lines) -> str:
        """The code of every branch of a conditional, or of its first branch alone when a branch opens a block that a later branch closes, such as two heads of one function for two systems."""
        branches = []
        while True:
            code, end = NamespaceScope.branch(lines)
            branches.append(code)
            if end in ("endif", None):
                break
        balanced = all(code.count("{") == code.count("}") for code in branches)
        return "\n".join(branches) if balanced else branches[0]

    @staticmethod
    def skip_string(text: str, index: int, code: list[str]) -> int:
        """Skips a string literal, raw ones included, and keeps only the string of a language linkage, so the scan still tells `extern "C"` apart."""
        prefix = re.search(r"(?:u8|[uUL])?R$", text[max(0, index - 3) : index])
        if prefix:
            delimiter = text[index + 1 : text.index("(", index)]
            end = text.index(f'){delimiter}"', index) + len(delimiter) + 2
            code.append('""')
            return end
        end = index + 1
        while end < len(text) and text[end] != '"':
            end += 2 if text[end] == "\\" else 1
        content = text[index + 1 : end]
        code.append('"C"' if content == "C" else '""')
        return end + 1

    @staticmethod
    def is_digit_separator(text: str, index: int) -> bool:
        """Whether an apostrophe separates the digits of a number, such as the one of `1'000`, rather than opening a character literal."""
        start = index
        while start > 0 and (text[start - 1].isalnum() or text[start - 1] in "._"):
            start -= 1
        return start < index and text[start].isdigit()

    @staticmethod
    def matching_brace(code: str, index: int) -> int:
        depth = 0
        for position in range(index, len(code)):
            if code[position] == "{":
                depth += 1
            elif code[position] == "}":
                depth -= 1
                if depth == 0:
                    return position
        raise ValueError("A brace of the source has no closing brace.")

    @staticmethod
    def statements(code: str) -> list[tuple[str, bool]]:
        """The statements at namespace scope, an anonymous namespace among them as `namespace {}`, each with whether it lies in a block of C linkage, where only entry points live."""
        found = []
        scopes: list[bool] = []
        statement = ""
        index = 0
        while index < len(code):
            character = code[index]
            if character == ";":
                found.append((statement, any(scopes)))
                statement = ""
                index += 1
                continue
            if character == "}":
                scopes.pop()
                statement = ""
                index += 1
                continue
            if character != "{":
                statement += character
                index += 1
                continue

            head = NamespaceScope.ATTRIBUTE.sub("", statement).strip()
            namespace = NamespaceScope.NAMESPACE.match(head)
            if namespace or head == 'extern "C"':
                if namespace and not namespace.group(1):
                    found.append(("namespace {}", any(scopes)))
                scopes.append(not namespace)
                statement = ""
                index += 1
                continue

            # Every other block belongs to the statement: a body ends it, while an initializer or a member initializer goes on until the next semicolon or block.
            end = NamespaceScope.matching_brace(code, index)
            opens_requirements = NamespaceScope.REQUIRES_EXPRESSION.search(statement)
            statement += "{}"
            index = end + 1
            following = code[index:].lstrip()
            if not opens_requirements and (not following or NamespaceScope.BLOCK_ENDS_STATEMENT.match(following[0])):
                found.append((statement, any(scopes)))
                statement = ""
        return found

    @staticmethod
    def problem(statement: str, linkage: bool) -> str | None:
        """What a statement at namespace scope declares that the code rules forbid there, or nothing when it declares a namespace, a type, an alias, a member of a class or an entry point."""
        head = " ".join(NamespaceScope.ATTRIBUTE.sub("", statement).split())
        if head == "namespace {}":
            return "an anonymous namespace"

        head = NamespaceScope.REQUIRES_CLAUSE.sub("", NamespaceScope.without_template_head(head))
        if NamespaceScope.MACRO_CALL.match(head):
            return None

        # Macros that annotate a declaration, such as the ones of nullability and of the names that Swift sees, declare nothing themselves.
        head = " ".join(NamespaceScope.MACRO_WORD.sub("", head).split())
        if not head or head == "{}" or NamespaceScope.NAMESPACE.match(head) or NamespaceScope.ALLOWED_HEADS.match(head):
            return None
        if NamespaceScope.CLASS_HEAD.match(head) or linkage or NamespaceScope.LINKAGE.match(head):
            return None

        name, is_function = NamespaceScope.declarator(head)
        if name is None or NamespaceScope.is_member(name):
            return None
        if is_function:
            return None if NamespaceScope.ENTRY_POINTS.match(name) else f'the function "{name}"'
        return f'the variable "{name}"'

    @staticmethod
    def without_template_head(head: str) -> str:
        while NamespaceScope.TEMPLATE_HEAD.match(head):
            depth = 0
            for position in range(head.index("<"), len(head)):
                depth += {"<": 1, ">": -1}.get(head[position], 0)
                if depth == 0:
                    head = head[position + 1 :].strip()
                    break
            else:
                return ""
        return head

    @staticmethod
    def declarator(head: str) -> tuple[str | None, bool]:
        """The name that a declaration declares and whether it declares a function, from the first parameter list or initializer outside every bracket."""
        operator = NamespaceScope.OPERATOR.search(head)
        if operator:
            qualifier = NamespaceScope.QUALIFIER.search(head[: operator.start()].rstrip())
            prefix = re.sub(r"\s+", "", qualifier.group(1)) if qualifier else ""
            return prefix + " ".join(operator.group(0).split()), True

        depth = 0
        for position, character in enumerate(head):
            if character in "([{" and depth == 0 and character != "[":
                before = head[:position].rstrip()
                if character == "{":
                    return NamespaceScope.name_before(before), False
                after = head[position + 1 :].lstrip()
                if after[:1] in "*&^":
                    pointer = re.match(r"[*&^]+\s*([\w:]+)", after)
                    return (pointer.group(1) if pointer else None), False
                return NamespaceScope.name_before(before), True
            if character == "=" and depth == 0 and head[position + 1 : position + 2] != "=":
                return NamespaceScope.name_before(head[:position].rstrip()), False
            if character == "<" and position > 0 and (head[position - 1].isalnum() or head[position - 1] in "_ "):
                depth += 1
            elif character == ">" and depth > 0:
                depth -= 1
            elif character in "([":
                depth += 1
            elif character in ")]" and depth > 0:
                depth -= 1
        return NamespaceScope.name_before(head.rstrip()), False

    @staticmethod
    def name_before(text: str) -> str | None:
        found = NamespaceScope.DECLARATOR.search(text)
        return re.sub(r"\s+", "", found.group(1)) if found else None

    @staticmethod
    def is_member(name: str) -> bool:
        """Whether a qualified name names a member of a class, whose name is in PascalCase or carries template arguments, rather than something of a namespace, whose name is in lowercase."""
        parts = re.sub(r"<.*>", "<>", name).split("::")
        return len(parts) > 1 and (parts[-2][:1].isupper() or parts[-2].endswith("<>"))

    @staticmethod
    def problems(text: str) -> list[str]:
        code = NamespaceScope.stripped(text)
        found = []
        for statement, linkage in NamespaceScope.statements(code):
            problem = NamespaceScope.problem(statement, linkage)
            if problem:
                found.append(problem)
        return found
