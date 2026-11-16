from haylen.core.frozen import Frozen
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.property_lists import PropertyLists
from haylen.platforms.generated_tree import GeneratedTree
from haylen.plugins.merged_contribution import MergedContribution


class AppExtension:
    """The XcodeGen target of an app extension that a plugin adds, which builds the sources of the extension with the identity of the app and the bundle identifier of the app followed by the name of the extension in lowercase, with the Info.plist and the entitlements that its contribution gives. The app embeds it."""

    # The kinds of app extension with the XcodeGen type of each and the Info.plist key that names its extension point.
    KINDS = {
        "extensionKit": ("extensionkit-extension", "EXAppExtensionAttributes", "EXExtensionPointIdentifier"),
        "appExtension": ("app-extension", "NSExtension", "NSExtensionPointIdentifier"),
    }

    @staticmethod
    def target(tree: GeneratedTree, extension: MergedContribution, target: AppleTarget) -> tuple[str, dict]:
        """Writes the Info.plist and the entitlements of an extension into `extensions/<name>/` of a generated project and returns its name with its XcodeGen target."""
        fields = Frozen.thaw(extension.fields)
        name = str(fields["name"])
        kind, attributes, point = AppExtension.KINDS[str(fields["kind"])]
        info = {
            "CFBundleDevelopmentRegion": "$(DEVELOPMENT_LANGUAGE)",
            "CFBundleDisplayName": name,
            "CFBundleExecutable": "$(EXECUTABLE_NAME)",
            "CFBundleIdentifier": "$(PRODUCT_BUNDLE_IDENTIFIER)",
            "CFBundleInfoDictionaryVersion": "6.0",
            "CFBundleName": "$(PRODUCT_NAME)",
            "CFBundlePackageType": "$(PRODUCT_BUNDLE_PACKAGE_TYPE)",
            "CFBundleShortVersionString": "$(MARKETING_VERSION)",
            "CFBundleVersion": "$(CURRENT_PROJECT_VERSION)",
            **fields.get("infoPlist", {}),
        }
        info[attributes] = {point: fields["point"], **info.get(attributes, {})}
        folder = f"extensions/{name}"
        tree.write_bytes(f"{folder}/Info.plist", PropertyLists.dumps(info))

        settings = {
            "PRODUCT_NAME": name,
            "PRODUCT_MODULE_NAME": name,
            "PRODUCT_BUNDLE_IDENTIFIER": f"$(HAYLEN_BUNDLE_IDENTIFIER).{name.lower()}",
            "INFOPLIST_FILE": str(tree.root / folder / "Info.plist"),
            "GENERATE_INFOPLIST_FILE": "NO",
            "SWIFT_OBJC_BRIDGING_HEADER": "",
            "SKIP_INSTALL": "YES",
        }
        if fields.get("entitlements"):
            tree.write_bytes(f"{folder}/{name}.entitlements", PropertyLists.dumps(fields["entitlements"]))
            settings["CODE_SIGN_ENTITLEMENTS"] = str(tree.root / folder / f"{name}.entitlements")
        spec: dict = {
            "type": kind,
            "platform": target.xcode_platform,
            "configFiles": {"Debug": str(tree.root / "App.xcconfig"), "Release": str(tree.root / "App.xcconfig")},
            "sources": [{"path": str(extension.folders[0] / str(fields["sources"])), "name": name, "group": "extensions", "type": "syncedFolder"}],
            "dependencies": [{"sdk": framework} for framework in fields.get("frameworks", [])],
            "settings": {"base": settings},
        }
        if "minimumVersion" in fields:
            spec["deploymentTarget"] = fields["minimumVersion"]
        return name, spec
