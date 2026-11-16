import subprocess

from haylen.core.repository import Repository


class TrackedFiles:
    """The files of the repository that the rule tests read: the tracked ones and the new ones that git does not ignore, so a file that a change adds counts before it is committed."""

    @staticmethod
    def names(*patterns: str) -> list[str]:
        command = ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", *patterns]
        listed = subprocess.run(command, cwd=Repository.ROOT, capture_output=True, text=True, check=True).stdout.splitlines()
        return sorted(name for name in set(listed) if (Repository.ROOT / name).is_file())
