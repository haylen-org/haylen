import json
from dataclasses import dataclass
from pathlib import Path

from haylen.platforms.product_folders import ProductFolders


@dataclass(frozen=True)
class DevelopmentRecord:
    """The file in the build folder of an app that names the development server of its run with its token, readable by its owner alone, through which `haylen app capture` of another terminal reaches the run. The server removes it when it closes."""

    FILE = "development.json"

    address: str
    token: str

    @staticmethod
    def path(app: Path) -> Path:
        return ProductFolders.app_folder(app) / DevelopmentRecord.FILE

    @staticmethod
    def write(app: Path, host: str, port: int, token: str) -> Path:
        """Writes the record of a server that listens on a host and a port, which this machine reaches at its loopback address when the server listens on every address."""
        reached = "127.0.0.1" if host in ("0.0.0.0", "") else host
        record = DevelopmentRecord.path(app)
        record.parent.mkdir(parents=True, exist_ok=True)
        record.write_text(json.dumps({"address": f"http://{reached}:{port}", "token": token}) + "\n", encoding="utf-8")
        record.chmod(0o600)
        return record

    @staticmethod
    def read(app: Path) -> DevelopmentRecord | None:
        """Reads the record of the run of an app, or nothing while no run serves its development server."""
        record = DevelopmentRecord.path(app)
        if not record.is_file():
            return None
        try:
            document = json.loads(record.read_text(encoding="utf-8"))
            return DevelopmentRecord(document["address"], document["token"])
        except json.JSONDecodeError, KeyError, TypeError:
            return None
