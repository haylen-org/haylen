class JavaProperties:
    """Writes and reads the values of Java properties files, which Gradle and the JDK read as ISO 8859-1 text with backslash escapes, so any text survives a round trip."""

    @staticmethod
    def escape(value: str) -> str:
        units = value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").encode("utf-16-be")
        codes = (int.from_bytes(units[index : index + 2], "big") for index in range(0, len(units), 2))
        return "".join(chr(code) if code < 0x80 else f"\\u{code:04x}" for code in codes)

    @staticmethod
    def line(key: str, value: str) -> str:
        return f"{key}={JavaProperties.escape(value)}"

    @staticmethod
    def read(text: str) -> dict[str, str]:
        """Reads the entries of a properties file that `escape` wrote, keeping the trailing spaces of a value as Java does."""
        values = {}
        for line in text.splitlines():
            stripped = line.lstrip()
            if not stripped or stripped.startswith(("#", "!")) or "=" not in stripped:
                continue
            key, value = stripped.split("=", 1)
            decoded = value.lstrip().encode("latin-1").decode("unicode_escape")
            values[key.strip()] = decoded.encode("utf-16", "surrogatepass").decode("utf-16")
        return values
