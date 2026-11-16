import os
import platform


class Host:
    """The machine that runs the tooling, whose system and architecture decide which targets it builds and which host tools it runs."""

    SYSTEMS = {"darwin": "macos"}
    ARCHITECTURES = {"aarch64": "arm64", "arm64": "arm64", "amd64": "x86_64", "x86_64": "x86_64"}

    @staticmethod
    def system() -> str:
        """The system in the words of targets, such as `macos`, `linux`, `windows` or `freebsd`."""
        name = platform.system().lower()
        return Host.SYSTEMS.get(name, name)

    @staticmethod
    def architecture() -> str:
        machine = platform.machine().lower()
        return Host.ARCHITECTURES.get(machine, machine)

    @staticmethod
    def executable(name: str) -> str:
        return f"{name}.exe" if Host.system() == "windows" else name

    @staticmethod
    def default_jobs() -> int:
        return max(1, (os.cpu_count() or 2) - 1)
