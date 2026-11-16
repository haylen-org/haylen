from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.missing_keys import MissingKeys
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.android.java_properties import JavaProperties
from haylen.toolchains.keytool import Keytool


@dataclass(frozen=True)
class AndroidSigningKey:
    """A key that signs the builds of one build type of an Android app: the upload key of release builds or a debug key that replaces the debug key of the Android SDK. It comes from `platform/android/keystore/<type>.properties` of the app, which `haylen app sign` writes, or else from what continuous integration gives: the Gradle properties `haylen.<type>.<value>` of the Gradle home, or else the environment variables `HAYLEN_<TYPE>_<VALUE>`. The build hands it to Gradle through the environment, so no password reaches a file of the generated project, a command line or a log."""

    FOLDER = Path("platform") / "android" / "keystore"
    # The values of a key, each with the suffix of its environment variable.
    VARIABLES = {"storeFile": "STORE_FILE", "storePassword": "STORE_PASSWORD", "keyAlias": "KEY_ALIAS", "keyPassword": "KEY_PASSWORD"}
    KINDS = ("release", "debug")
    DEFAULT_ALIAS = "upload"
    DEFAULT_PASSWORD = "upload"
    DEFAULT_NAME = "CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR"

    kind: str
    store_file: Path
    store_password: str
    key_alias: str
    key_password: str

    @staticmethod
    def files(app_folder: Path, kind: str) -> dict[str, Path]:
        """The keystore of a key, its certificate and the properties that name them, in the key folder of the app."""
        folder = app_folder / AndroidSigningKey.FOLDER
        return {"keystore": folder / f"{kind}.jks", "certificate": folder / f"{kind}.pem", "properties": folder / f"{kind}.properties"}

    @staticmethod
    def properties(kind: str, alias: str, password: str) -> str:
        """The properties of a key, with its keystore next to them."""
        values = {"storeFile": f"{kind}.jks", "storePassword": password, "keyAlias": alias, "keyPassword": password}
        lines = [f'# Written by "haylen app sign". It holds the passwords of the {kind} key, so the folder stays out of the repository.']
        return "\n".join([*lines, *(JavaProperties.line(key, value) for key, value in values.items())]) + "\n"

    @staticmethod
    def find(app_folder: Path, kind: str, environment: Mapping[str, str]) -> AndroidSigningKey | None:
        """Returns the key of a build type, or nothing when neither the app nor continuous integration gives one. A relative keystore is relative to the app folder."""
        properties = AndroidSigningKey.files(app_folder, kind)["properties"]
        if properties.is_file():
            values = JavaProperties.read(properties.read_text(encoding="latin-1"))
            missing = [name for name in AndroidSigningKey.VARIABLES if not values.get(name)]
            if missing:
                raise MissingKeys(f'The file `{Repository.shown(properties)}` has no "{missing[0]}". Create the key again with "haylen app sign --target android --kind {kind} --option force=true".')
            return AndroidSigningKey.of(kind, values, properties.parent)

        home = Path(environment.get("GRADLE_USER_HOME", Path(environment.get("HOME", Path.home())) / ".gradle"))
        gradle = home / "gradle.properties"
        given_properties = JavaProperties.read(gradle.read_text(encoding="latin-1")) if gradle.is_file() else {}
        given = {name: given_properties.get(f"haylen.{kind}.{name}") or environment.get(f"HAYLEN_{kind.upper()}_{suffix}") for name, suffix in AndroidSigningKey.VARIABLES.items()}
        if not all(given.values()):
            return None
        return AndroidSigningKey.of(kind, {name: str(value) for name, value in given.items()}, app_folder)

    @staticmethod
    def of(kind: str, values: Mapping[str, str], folder: Path) -> AndroidSigningKey:
        return AndroidSigningKey(kind, folder / values["storeFile"], values["storePassword"], values["keyAlias"], values["keyPassword"])

    @staticmethod
    def release(app_folder: Path, environment: Mapping[str, str]) -> AndroidSigningKey:
        """Returns the upload key that signs the release builds, and stops with the ways to give one."""
        key = AndroidSigningKey.find(app_folder, "release", environment)
        if key is not None:
            return key
        properties = ", ".join(f'"haylen.release.{name}"' for name in AndroidSigningKey.VARIABLES)
        variables = ", ".join(f'"HAYLEN_RELEASE_{suffix}"' for suffix in AndroidSigningKey.VARIABLES.values())
        raise MissingKeys(
            f'The release build needs the upload key of the app. Create it with "haylen app sign --target android --kind release" and the app folder, or give the Gradle properties {properties} of the Gradle home or the environment variables {variables}.'
        )

    def environment(self) -> dict[str, str]:
        """The variables that hand the key to the Gradle scripts of the generated project."""
        values = {"storeFile": str(self.store_file), "storePassword": self.store_password, "keyAlias": self.key_alias, "keyPassword": self.key_password}
        return {f"HAYLEN_{self.kind.upper()}_{suffix}": values[name] for name, suffix in AndroidSigningKey.VARIABLES.items()}

    @staticmethod
    def create(context: ToolingContext, app_folder: Path, kind: str, options: Mapping[str, object]) -> tuple[str, ...]:
        """Creates the key of a build type with `keytool`, exports its certificate as PEM and writes its properties. An existing key stays unless the option `force` replaces it, since an app signed with another key cannot update the installed one."""
        if kind not in AndroidSigningKey.KINDS:
            raise InvalidOption(f'Android signs with a "release" or a "debug" key, not with a "{kind}" key.')
        unknown = sorted(set(options) - {"alias", "password", "name", "force"})
        if unknown:
            raise InvalidOption(f'The signing of Android takes the options "alias", "password", "name" and "force", not "{unknown[0]}".')
        alias = str(options.get("alias") or AndroidSigningKey.DEFAULT_ALIAS)
        password = str(options.get("password") or AndroidSigningKey.DEFAULT_PASSWORD)
        name = str(options.get("name") or AndroidSigningKey.DEFAULT_NAME)
        if len(password) < Keytool.PASSWORD_LENGTH:
            raise InvalidOption(f'The password of the key needs at least {Keytool.PASSWORD_LENGTH} characters, which "keytool" asks of every keystore.')

        force = options.get("force", "false")
        if force not in ("true", "false"):
            raise InvalidOption(f'The option "force" is "true" or "false", not "{force}". Give "--option force=true" to replace an existing key.')

        files = AndroidSigningKey.files(app_folder, kind)
        existing = [path for path in files.values() if path.exists()]
        if existing and force != "true":
            raise InvalidOption(
                f'The app already has a {kind} key in `{Repository.shown(existing[0])}`. Keep it, since an app signed with another key cannot update the installed one, or replace it with "--option force=true".'
            )
        keytool = Keytool(context)
        for path in existing:
            path.unlink()

        context.terminal.step(f"Creating the {kind} key of `{Repository.shown(app_folder)}`")
        keytool.create(files["keystore"], alias, password, name)
        keytool.export_certificate(files["keystore"], alias, password, files["certificate"])
        files["properties"].write_text(AndroidSigningKey.properties(kind, alias, password), encoding="utf-8")
        files["properties"].chmod(0o600)
        context.terminal.success(f"Created the {kind} key `{Repository.shown(files['keystore'])}` with its certificate `{Repository.shown(files['certificate'])}`.")
        context.terminal.info(f"The {kind} builds of the app sign with it, and the folder `{Repository.shown(files['keystore'].parent)}` stays out of the repository.")
        return tuple(str(path) for path in files.values())
