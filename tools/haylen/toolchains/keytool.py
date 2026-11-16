from pathlib import Path

from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.java import Java


class Keytool:
    """The `keytool` of the JDK, which creates the RSA keys that sign Android apps in PKCS12 keystores and exports their certificates. The password reaches it through a variable of its environment, so neither the printed commands nor the list of processes show it."""

    KEY_SIZE = 2048
    KEY_DAYS = 10000
    # The command refuses passwords shorter than this.
    PASSWORD_LENGTH = 6
    PASSWORD_VARIABLE = "HAYLEN_KEY_PASSWORD"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context
        self.tool = Java(context.environment).tool("keytool")

    def environment(self, password: str) -> dict[str, str]:
        return {**self.context.environment, Keytool.PASSWORD_VARIABLE: password}

    def create(self, keystore: Path, alias: str, password: str, name: str) -> None:
        """Creates a keystore that holds one key of the alias, whose certificate names `name`, such as `CN=Upload, O=Example`."""
        keystore.parent.mkdir(parents=True, exist_ok=True)
        command: list[object] = [self.tool, "-genkeypair", "-keystore", keystore, "-storetype", "PKCS12", "-alias", alias, "-keyalg", "RSA"]
        command += ["-keysize", Keytool.KEY_SIZE, "-validity", Keytool.KEY_DAYS, "-dname", name, "-storepass:env", Keytool.PASSWORD_VARIABLE]
        self.context.process.run(command, env=self.environment(password))

    def export_certificate(self, keystore: Path, alias: str, password: str, certificate: Path) -> None:
        """Writes the certificate of a key as PEM, which stores and services ask for."""
        command: list[object] = [self.tool, "-exportcert", "-rfc", "-keystore", keystore, "-alias", alias, "-file", certificate, "-storepass:env", Keytool.PASSWORD_VARIABLE]
        self.context.process.run(command, env=self.environment(password))
