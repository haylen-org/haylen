import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class EngineBoundaryTest:
    """The engine holds only what every app needs to run, so it owns, asks for and answers no permission, shows nothing in the controls of what plays, owns no delegate of the notification center, reads no location, camera or Bluetooth device, opens no share sheet or picker and vibrates no device, while it rumbles gamepads, declares no permission in its Android libraries, adds no usage description, background mode or entitlement to the Apple projects it generates and links no framework that it does not use. The capabilities that only some apps need live in plugins, which declare what they need themselves."""

    SOURCES = ("engine/include", "engine/src", "engine/lua", "engine/platform")
    SOURCE_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".mm", ".m", ".swift", ".java", ".kt", ".js")
    FORBIDDEN = re.compile(r"Permission::|requestPermission|registerPermission|HaylenPermission|UNUserNotificationCenter|MPNowPlayingInfoCenter|navigator\.mediaSession")
    # The APIs of the capabilities that plugins own: the location, the camera, Bluetooth, the share sheets, the pickers of files and photos, and the vibration of the device, whose gamepads the engine still rumbles.
    PLUGIN_APIS = re.compile(
        r"\b(?:CLLocationManager|CoreLocation/|LocationManager|navigator\.geolocation|AVCaptureDevice|AVCaptureSession|android\.hardware\.camera2|CameraManager|getUserMedia"
        r"|CoreBluetooth/|CBCentralManager|BluetoothAdapter|BluetoothManager|navigator\.bluetooth|UIActivityViewController|NSSharingService\w*|Intent\.ACTION_SEND\w*|navigator\.share"
        r"|UIImagePickerController|PHPicker\w*|UIDocumentPicker\w*|NSOpenPanel|NSSavePanel|ACTION_OPEN_DOCUMENT\w*|ACTION_GET_CONTENT|show(?:Open|Save)FilePicker|showDirectoryPicker|GetOpenFileName\w*|GetSaveFileName\w*|IFileOpenDialog|IFileSaveDialog|SDL_Show\w*FileDialog"
        r"|UI(?:Impact|Notification|Selection)FeedbackGenerator|CHHapticEngine|VIBRATOR_(?:MANAGER_)?SERVICE|performHapticFeedback|navigator\.vibrate)\b"
    )
    APPLE_TEMPLATE = "tools/haylen/platforms/apple/template"
    APPLE_DECLARATIONS = re.compile(r"NS\w+UsageDescription|UIBackgroundModes")
    FRAMEWORK_LISTS = re.compile(r"^set\(HAYLEN_APPLE_FRAMEWORKS_\w+ (.*)\)$", re.MULTILINE)
    # Frameworks that no code of the engine uses, which an app links through a plugin that needs them.
    UNUSED_FRAMEWORKS = frozenset({"MetalKit", "MediaPlayer", "UserNotifications"})

    @staticmethod
    def capabilities_of_plugins(line: str) -> list[str]:
        """The names of the capabilities of plugins that a line of a source uses. A comment may name an API as an example, such as the class that a requirement checks."""
        found = [match.group(0) for match in EngineBoundaryTest.FORBIDDEN.finditer(line)]
        if not line.lstrip().startswith(("//", "*", "/*")):
            found += [match.group(0) for match in EngineBoundaryTest.PLUGIN_APIS.finditer(line)]
        return found

    def test_engine_sources_name_no_capability_of_a_plugin(self):
        found = []
        for name in TrackedFiles.names(*EngineBoundaryTest.SOURCES):
            if not name.endswith(EngineBoundaryTest.SOURCE_SUFFIXES):
                continue
            for number, line in enumerate((Repository.ROOT / name).read_text(encoding="utf-8").splitlines(), start=1):
                found += [f"{name}:{number}: {match}" for match in EngineBoundaryTest.capabilities_of_plugins(line)]
        assert found == []

    def test_the_scan_finds_the_capabilities_of_plugins_and_leaves_gamepad_rumble(self):
        lines = (
            "CLLocationManager* manager = [[CLLocationManager alloc] init];",
            "navigator.geolocation.getCurrentPosition(found);",
            "CameraManager cameras = (CameraManager) getSystemService(Context.CAMERA_SERVICE);",
            "CBCentralManager* central = nil;",
            "Intent share = new Intent(Intent.ACTION_SEND);",
            "NSOpenPanel* panel = [NSOpenPanel openPanel];",
            "await window.showOpenFilePicker();",
            "Vibrator vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);",
            "UIImpactFeedbackGenerator* feedback = nil;",
        )
        assert [line for line in lines if not EngineBoundaryTest.capabilities_of_plugins(line)] == []
        kept = ("VibratorManager manager = device.getVibratorManager();", "editor.imeOptions = EditorInfo.IME_ACTION_SEND;", "// A class such as `CLLocationManager` of `CoreLocation.framework`.")
        assert [line for line in kept if EngineBoundaryTest.capabilities_of_plugins(line)] == []

    def test_the_android_libraries_of_the_engine_declare_no_permission(self):
        manifests = [name for name in TrackedFiles.names("engine/platform/android") if name.endswith("AndroidManifest.xml")]
        assert manifests
        assert [name for name in manifests if "uses-permission" in (Repository.ROOT / name).read_text(encoding="utf-8")] == []

    def test_the_apple_templates_declare_no_usage_background_mode_or_entitlement(self):
        files = TrackedFiles.names(EngineBoundaryTest.APPLE_TEMPLATE)
        assert [name for name in files if name.endswith(".entitlements")] == []
        found = [name for name in files if name.endswith((".plist", ".yml", ".json")) and EngineBoundaryTest.APPLE_DECLARATIONS.search((Repository.ROOT / name).read_text(encoding="utf-8"))]
        assert found == []

    def test_the_apple_framework_lists_name_no_framework_that_the_engine_does_not_use(self):
        text = (Repository.ENGINE / "cmake" / "haylen-app.cmake").read_text(encoding="utf-8")
        listed = {name for line in EngineBoundaryTest.FRAMEWORK_LISTS.findall(text) for name in line.split() if not name.startswith("$")}
        assert listed
        assert listed & EngineBoundaryTest.UNUSED_FRAMEWORKS == set()
