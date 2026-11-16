from collections.abc import Mapping

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.platforms.android.android_template import AndroidTemplate


class AndroidManifestFile:
    """Writes the manifest of the app module from the template: the permissions that every app gets unless `platforms.android.manifest.removedPermissions` of `haylen.json` names them, which the merged manifest then leaves out whichever library brings them, and the ones the app adds, the attributes of the application with the class of the application and the ones the app gives, the orientation, the status bar and the library that the activity loads."""

    PATH = "app/src/main/AndroidManifest.xml"
    # The permissions of the template, which network access, the network events, the rumble of gamepads and the local network need.
    DEFAULT_PERMISSIONS = ("android.permission.INTERNET", "android.permission.ACCESS_NETWORK_STATE", "android.permission.VIBRATE", "android.permission.ACCESS_LOCAL_NETWORK")
    ORIENTATIONS = {"landscape": "sensorLandscape", "portrait": "sensorPortrait", "any": "fullSensor"}

    @staticmethod
    def manifest_section(product: ResolvedProduct) -> Mapping[str, object]:
        section = product.primary.setting(AndroidContributions.SECTION, "manifest", {})
        return Frozen.thaw(section)

    @staticmethod
    def removed_permissions(product: ResolvedProduct) -> tuple[str, ...]:
        return tuple(AndroidManifestFile.manifest_section(product).get("removedPermissions", ()))

    @staticmethod
    def render(product: ResolvedProduct, library: str) -> str:
        app = product.app
        manifest = AndroidManifestFile.manifest_section(product)
        added = list(manifest.get("permissions", ()))
        removed = list(manifest.get("removedPermissions", ()))
        both = sorted(set(added) & set(removed))
        if both:
            raise InvalidManifest(f'The section "platforms.android.manifest" of `{app.origin}` both adds and removes the permission "{both[0]}". Keep it in one of the lists.', origin=app.origin)

        permissions = [permission for permission in (*AndroidManifestFile.DEFAULT_PERMISSIONS, *added) if permission not in removed]
        lines = [f'    <uses-permission android:name="{AndroidTemplate.xml(permission)}" />' for permission in permissions]
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
        for name, value in manifest.get("applicationAttributes", {}).items():
            attributes[name] = AndroidTemplate.xml(str(value))

        tokens = {
            "PERMISSIONS": "\n".join(lines),
            "APPLICATION_ATTRIBUTES": "\n".join(f'        android:{name}="{value}"' for name, value in attributes.items()),
            "ORIENTATION": AndroidManifestFile.ORIENTATIONS[str(app.document.get("orientation", "landscape"))],
            "LIBRARY": library,
            "STATUS_BAR": str(app.section("window").get("statusBar", "hidden")),
        }
        return AndroidTemplate.render(AndroidTemplate.PROJECT / AndroidManifestFile.PATH, tokens)
