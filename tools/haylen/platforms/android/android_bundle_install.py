import tempfile
from pathlib import Path

from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.android.android_signing_key import AndroidSigningKey
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.bundletool import Bundletool
from haylen.toolchains.java import Java


class AndroidBundleInstall:
    """Installs the bundle of an Android app on a device in the local testing mode of Google Play with bundletool, signed with the upload key of the app: the device receives the APKs of its configuration, and the asset packs that Play delivers after the install wait on the device, where the library of Play delivers them as the store would. The passwords reach bundletool through files that only their owner reads, which never appear in a command line or a log."""

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def install(self, bundle: Path, app_folder: Path, serial: str, output: Path) -> Path:
        """Builds the APK set of the device from the bundle into `output` and installs it, and returns the APK set."""
        key = AndroidSigningKey.release(app_folder, self.context.environment)
        java = Java(self.context.environment).tool("java")
        tool = Bundletool.path(self.context)
        adb = AndroidSdk(self.context.environment).adb
        output.parent.mkdir(parents=True, exist_ok=True)
        output.unlink(missing_ok=True)

        self.context.terminal.step(f"Installing the bundle `{Repository.shown(bundle)}` in the local testing mode of Google Play")
        with tempfile.TemporaryDirectory() as temporary:
            passwords = {}
            for name, value in (("store", key.store_password), ("key", key.key_password)):
                passwords[name] = Path(temporary) / name
                passwords[name].touch(0o600)
                passwords[name].write_text(value, encoding="utf-8")
            command: list[object] = [java, "-jar", tool, "build-apks", f"--bundle={bundle}", f"--output={output}", "--local-testing", "--connected-device", f"--device-id={serial}", f"--adb={adb}"]
            command += [f"--ks={key.store_file}", f"--ks-pass=file:{passwords['store']}", f"--ks-key-alias={key.key_alias}", f"--key-pass=file:{passwords['key']}"]
            self.context.process.run(command, env=self.context.environment)
        self.context.process.run([java, "-jar", tool, "install-apks", f"--apks={output}", f"--device-id={serial}", f"--adb={adb}"], env=self.context.environment)
        return output
