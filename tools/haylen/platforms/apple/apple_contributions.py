from collections.abc import Mapping
from types import MappingProxyType

from haylen.core.frozen import Frozen
from haylen.platforms.apple.ios_devices import IosDevices
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.merge_mode import MergeMode
from haylen.platforms.override_section import OverrideSection
from haylen.plugins.core_contributions import CoreContributions


class AppleContributions:
    """The contribution types that plugins give the Apple platforms and the sections of `haylen.json` that the Apple backend reads. Paths of contributions are relative to the plugin folder, and a `file` parameter of a plugin names a file of the app instead."""

    PLATFORMS = frozenset({"ios", "catalyst", "tvos", "visionos", "macos"})
    # App extensions build for every Apple platform but Mac Catalyst, whose apps take none.
    EXTENSION_PLATFORMS = frozenset({"ios", "tvos", "visionos", "macos"})

    CLASS = "appleClass"
    SOURCES = "appleSources"
    SWIFT_PACKAGE = "swiftPackage"
    FRAMEWORK = "framework"
    BINARY_FRAMEWORK = "binaryFramework"
    INFO_PLIST = "infoPlist"
    ENTITLEMENTS = "entitlements"
    PRIVACY = "privacy"
    RESOURCE = "appleResource"
    BUILD_SCRIPT = "buildScript"
    APP_EXTENSION = "appExtension"
    ASSET_PACKS = "assetPacks"

    # The sections of `haylen.json`: `apple` for every Apple platform and one per platform.
    APPLE_SECTION = "apple"
    PLATFORM_SECTIONS = ("ios", "catalyst", "tvos", "visionos", "macos")

    PATH = {"type": "string", "minLength": 1, "pattern": "^[^/]", "description": "A path relative to the plugin folder."}
    NAMES = {"type": "array", "items": {"type": "string", "minLength": 1}, "uniqueItems": True}
    VALUES = {"type": "object", "additionalProperties": False, "required": ["values"], "properties": {"values": {"type": "object"}}}
    SYSTEM_FRAMEWORK = {"type": "string", "pattern": "^[A-Za-z0-9_+.-]+\\.(framework|tbd)$", "description": "A framework or library of the SDK, such as StoreKit.framework or libz.tbd."}

    @staticmethod
    def schema(required: list[str], properties: Mapping[str, object]) -> Mapping[str, object]:
        return Frozen.freeze({"type": "object", "additionalProperties": False, "required": required, "properties": dict(properties)})

    @staticmethod
    def types() -> Mapping[str, ContributionType]:
        platforms = AppleContributions.PLATFORMS
        path = AppleContributions.PATH
        names = AppleContributions.NAMES
        schema = AppleContributions.schema
        extension = {
            "name": {"type": "string", "pattern": "^[A-Z][A-Za-z0-9]*$", "description": "The name of the target and the product of the extension."},
            "kind": {"enum": ["appExtension", "extensionKit"], "description": "An app extension of NSExtension or an extension of ExtensionKit."},
            "point": {"type": "string", "minLength": 1, "description": "The identifier of the extension point."},
            "sources": path,
            "minimumVersion": {"type": "string", "pattern": "^\\d+(\\.\\d+){0,2}$"},
            "frameworks": {"type": "array", "items": AppleContributions.SYSTEM_FRAMEWORK, "uniqueItems": True},
            "infoPlist": {"type": "object"},
            "entitlements": {"type": "object"},
        }
        script = {
            "name": {"type": "string", "minLength": 1},
            "script": {"type": "string", "minLength": 1},
            "inputFiles": names,
            "outputFiles": names,
            "basedOnDependencyAnalysis": {
                "type": "boolean",
                "description": "Whether the script runs only when its inputs changed or its outputs are missing, true by default. A script that writes no file sets it to false, so it runs on every build.",
            },
        }
        package = {
            "name": {"type": "string", "minLength": 1},
            "url": {"type": "string", "pattern": "^(https://|ssh://|git@)\\S+$"},
            "exactVersion": {"type": "string", "pattern": "^\\d+\\.\\d+\\.\\d+([-+][0-9A-Za-z.-]+)?$"},
            "products": {**names, "minItems": 1},
        }
        kinds = (
            (
                AppleContributions.CLASS,
                schema(["name"], {"name": {"type": "string", "pattern": "^[A-Za-z_][A-Za-z0-9_.]*$"}}),
                (),
                MergeMode.ACCUMULATE,
                "The Objective-C runtime name of the plugin class, which the engine creates.",
            ),
            (AppleContributions.SOURCES, schema(["path"], {"path": path}), (), MergeMode.ACCUMULATE, "A folder or file of Swift, Objective-C, Objective-C++, C or C++ sources that the app compiles."),
            (AppleContributions.FRAMEWORK, schema(["name"], {"name": AppleContributions.SYSTEM_FRAMEWORK}), ("name",), MergeMode.UNIQUE, "A framework or library of the SDK that the app links."),
            (
                AppleContributions.BINARY_FRAMEWORK,
                schema(["path", "embed"], {"path": {**path, "pattern": "^[^/].*\\.(xcframework|framework)$"}, "embed": {"type": "boolean"}}),
                ("path",),
                MergeMode.UNIQUE,
                "A framework that the plugin ships, which the app embeds when it is dynamic.",
            ),
            (AppleContributions.INFO_PLIST, AppleContributions.VALUES, (), MergeMode.DICTIONARY, "Keys of the Info.plist of the app."),
            (AppleContributions.ENTITLEMENTS, AppleContributions.VALUES, (), MergeMode.DICTIONARY, "Entitlements that the app is signed with."),
            (AppleContributions.PRIVACY, AppleContributions.VALUES, (), MergeMode.DICTIONARY, "Keys of the privacy manifest of the app."),
            (AppleContributions.RESOURCE, schema(["path"], {"path": path}), (), MergeMode.ACCUMULATE, "A file or bundle that the app holds at the root of its resources."),
            (AppleContributions.BUILD_SCRIPT, schema(["name", "script"], script), (), MergeMode.ACCUMULATE, "A shell script that runs after the app builds."),
            (AppleContributions.ASSET_PACKS, schema([], {"prefetch": names}), (), MergeMode.EXCLUSIVE, "The delivery of the named packs of the content as asset packs of Background Assets."),
        )
        types = {name: ContributionType(name, platforms, MappingProxyType(dict(schema_of)), identity, merge, description) for name, schema_of, identity, merge, description in kinds}
        # Plugins that share a Swift package at one version link the products that each one needs.
        types[AppleContributions.SWIFT_PACKAGE] = ContributionType(
            AppleContributions.SWIFT_PACKAGE,
            platforms,
            schema(["name", "url", "exactVersion", "products"], package),
            ("url",),
            MergeMode.UNIQUE,
            "A Swift package at one exact version, whose products the app links.",
            ("products",),
        )
        types[AppleContributions.APP_EXTENSION] = ContributionType(
            AppleContributions.APP_EXTENSION,
            AppleContributions.EXTENSION_PLATFORMS,
            schema(["name", "kind", "point", "sources"], extension),
            ("name",),
            MergeMode.UNIQUE,
            "An app extension that the app embeds, built from sources of the plugin.",
        )
        types[CoreContributions.NATIVE_LIBRARY] = CoreContributions.native_library(platforms)
        return MappingProxyType(types)

    @staticmethod
    def sections() -> Mapping[str, OverrideSection]:
        settings = {
            "type": "object",
            "additionalProperties": {"anyOf": [{"type": ["string", "number", "boolean"]}, {"type": "array", "items": {"type": "string"}}]},
            "description": "Xcode build settings of the app target in every configuration, which win over the ones of the backend. A setting that another owner settles, such as the architectures, the oldest system version, the devices or the identity of the app, takes only the value of that owner, and the flags of OTHER_LDFLAGS join the flags of the backend.",
        }
        apple = {
            "type": "object",
            "additionalProperties": False,
            "properties": {
                "sources": {
                    **AppleContributions.NAMES,
                    "description": "Folders and files of Swift, Objective-C, Objective-C++, C and C++ sources of the app, relative to the app folder, such as its bridge handlers.",
                },
                "team": {"type": "string", "pattern": "^[A-Z0-9]{10}$", "description": "The development team that signs the apps of devices."},
                "buildSettings": settings,
                "privacy": {"type": "object", "description": "Keys of the privacy manifest of the app."},
            },
        }
        platform = {
            "type": "object",
            "additionalProperties": False,
            "properties": {"infoPlist": {"type": "object"}, "entitlements": {"type": "object"}},
        }
        sections = {
            AppleContributions.APPLE_SECTION: OverrideSection(
                AppleContributions.APPLE_SECTION,
                AppleContributions.PLATFORMS,
                Frozen.freeze(apple),
                MappingProxyType({"privacy": AppleContributions.PRIVACY}),
                "The sources, team, build settings and privacy declarations of the apps of every Apple platform.",
            )
        }
        for name in AppleContributions.PLATFORM_SECTIONS:
            schema = platform
            description = f'The Info.plist keys and entitlements of the "{name}" app.'
            if name == IosDevices.SECTION:
                schema = {**platform, "properties": {**platform["properties"], IosDevices.KEY: IosDevices.SCHEMA}}
                description = 'The devices, Info.plist keys and entitlements of the "ios" app.'
            sections[name] = OverrideSection(
                name,
                frozenset({name}),
                Frozen.freeze(schema),
                MappingProxyType({"infoPlist": AppleContributions.INFO_PLIST, "entitlements": AppleContributions.ENTITLEMENTS}),
                description,
            )
        return MappingProxyType(sections)
