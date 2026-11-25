from collections.abc import Mapping

from haylen.core.app_category import AppCategory
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.platforms.android.android_template import AndroidTemplate


class AndroidManifestFile:
    """Writes the manifest of the app module from the template: the permissions that every app gets unless `platforms.android.manifest.removedPermissions` of `haylen.json` names them, which the merged manifest then leaves out whichever library brings them, and the ones the app adds, the features that the app uses, which it requires only where `features` says so, the attributes of the application with the class of the application, the category of `category` where Android has one, the game mode of a game and the ones the app gives, the attributes of the activity, the status bar and the library that the activity loads."""

    PATH = "app/src/main/AndroidManifest.xml"
    # The permissions of the template, which network access, the network events, the rumble of gamepads and the local network need.
    DEFAULT_PERMISSIONS = ("android.permission.INTERNET", "android.permission.ACCESS_NETWORK_STATE", "android.permission.VIBRATE")
    ORIENTATIONS = {"landscape": "sensorLandscape", "portrait": "sensorPortrait", "any": "fullSensor"}
    # The features that the app uses without requiring them, which `features` of the section requires or adds to.
    DEFAULT_FEATURES = {
        "android.hardware.touchscreen": False,
        "android.hardware.screen.portrait": False,
        "android.hardware.screen.landscape": False,
        "android.software.leanback": False,
        "android.hardware.gamepad": False,
    }
    # The attributes of the activity that the engine needs, each with the reason, which `activityAttributes` of the section may repeat but not change.
    FIXED_ACTIVITY = {
        "name": ("dev.haylen.HaylenActivity", "the engine, whose activity runs the app,"),
        "configChanges": (
            "orientation|screenSize|screenLayout|smallestScreenSize|keyboard|keyboardHidden|navigation|touchscreen|density|uiMode|colorMode|locale|layoutDirection|fontScale|fontWeightAdjustment|mcc|mnc",
            "the engine, which takes every change of the configuration itself instead of starting the app again,",
        ),
        "exported": ("true", "the launcher, which starts the activity from outside the app,"),
        "launchMode": ("singleTop", "the engine, whose single activity keeps the screens of plugins over the app when the launcher brings it back,"),
    }
    # The values of `android:appCategory`, which an app without a category and the other categories of an app leave out.
    CATEGORIES = {
        AppCategory.GAME: "game",
        AppCategory.PRODUCTIVITY: "productivity",
        AppCategory.GRAPHICS: "image",
        AppCategory.MUSIC: "audio",
        AppCategory.VIDEO: "video",
        AppCategory.SOCIAL: "social",
    }

    @staticmethod
    def manifest_section(product: ResolvedProduct) -> Mapping[str, object]:
        section = product.primary.setting(AndroidContributions.SECTION, "manifest", {})
        return Frozen.thaw(section)

    @staticmethod
    def removed_permissions(product: ResolvedProduct) -> tuple[str, ...]:
        return tuple(AndroidManifestFile.manifest_section(product).get("removedPermissions", ()))

    @staticmethod
    def permission_name(entry: object) -> str:
        """The name of a permission of `permissions`, which is the name alone or an object with its `name` and `maxSdkVersion`."""
        return str(entry["name"]) if isinstance(entry, Mapping) else str(entry)

    @staticmethod
    def permission_line(entry: object) -> str:
        name = AndroidTemplate.xml(AndroidManifestFile.permission_name(entry))
        if isinstance(entry, Mapping) and "maxSdkVersion" in entry:
            return f'    <uses-permission android:name="{name}" android:maxSdkVersion="{int(entry["maxSdkVersion"])}" />'
        return f'    <uses-permission android:name="{name}" />'

    @staticmethod
    def activity_attributes(product: ResolvedProduct, manifest: Mapping[str, object]) -> dict[str, str]:
        """The attributes of the activity: the ones that the engine needs, the orientation of `orientation` and the ones that `activityAttributes` gives, which fails with both owners for a value that contradicts the engine or `orientation`."""
        app = product.app
        orientation = str(app.document.get("orientation", "landscape"))
        owned = {name: value for name, (value, _) in AndroidManifestFile.FIXED_ACTIVITY.items()}
        owners = {name: owner for name, (_, owner) in AndroidManifestFile.FIXED_ACTIVITY.items()}
        owned["screenOrientation"] = AndroidManifestFile.ORIENTATIONS[orientation]
        owners["screenOrientation"] = f'"orientation" of `{app.origin}`, which is "{orientation}",'

        given = dict(manifest.get("activityAttributes", {}))
        problems = [
            f'The key "platforms.android.manifest.activityAttributes.{name}" is "{value}", while {owners[name]} gives "{owned[name]}".'
            for name, value in given.items()
            if name in owned and str(value) != owned[name]
        ]
        if problems:
            lines = "\n".join(f"  {problem}" for problem in problems)
            raise InvalidManifest(f"The activity attributes of `{app.origin}` contradict the values of other owners. Remove the keys, or make them agree:\n{lines}", origin=app.origin)
        return {**owned, **{name: AndroidTemplate.xml(str(value)) for name, value in given.items()}}

    @staticmethod
    def render(product: ResolvedProduct, library: str) -> str:
        app = product.app
        manifest = AndroidManifestFile.manifest_section(product)
        added = {AndroidManifestFile.permission_name(entry): entry for entry in manifest.get("permissions", ())}
        removed = list(manifest.get("removedPermissions", ()))
        both = sorted(set(added) & set(removed))
        if both:
            raise InvalidManifest(f'The section "platforms.android.manifest" of `{app.origin}` both adds and removes the permission "{both[0]}". Keep it in one of the lists.', origin=app.origin)

        permissions = {name: name for name in AndroidManifestFile.DEFAULT_PERMISSIONS} | added
        lines = [AndroidManifestFile.permission_line(entry) for name, entry in permissions.items() if name not in removed]
        lines += [f'    <uses-permission android:name="{AndroidTemplate.xml(permission)}" tools:node="remove" />' for permission in removed]

        attributes = {
            "banner": "@drawable/banner",
            "enableOnBackInvokedCallback": "true",
            "icon": "@mipmap/ic_launcher",
            "label": AndroidTemplate.text(app.name),
            "theme": "@style/Theme.Haylen.Splash",
        }
        application_class = product.primary.setting(AndroidContributions.SECTION, "applicationClass")
        if application_class is not None:
            attributes = {"name": str(application_class), **attributes}
        category = AndroidManifestFile.CATEGORIES.get(app.category) if app.category is not None else None
        if category is not None:
            attributes["appCategory"] = category
        # A game takes the game mode and the game dashboard of Android.
        if app.category is AppCategory.GAME:
            attributes["isGame"] = "true"
        for name, value in manifest.get("applicationAttributes", {}).items():
            attributes[name] = AndroidTemplate.xml(str(value))

        features = {**AndroidManifestFile.DEFAULT_FEATURES, **manifest.get("features", {})}
        tokens = {
            "PERMISSIONS": "\n".join(lines),
            "FEATURES": "\n".join(f'    <uses-feature android:name="{AndroidTemplate.xml(name)}" android:required="{str(required).lower()}" />' for name, required in features.items()),
            "APPLICATION_ATTRIBUTES": "\n".join(f'        android:{name}="{value}"' for name, value in attributes.items()),
            "ACTIVITY_ATTRIBUTES": "\n".join(f'            android:{name}="{value}"' for name, value in AndroidManifestFile.activity_attributes(product, manifest).items()),
            "LIBRARY": library,
            "STATUS_BAR": str(app.section("window").get("statusBar", "hidden")),
        }
        return AndroidTemplate.render(AndroidTemplate.PROJECT / AndroidManifestFile.PATH, tokens)
