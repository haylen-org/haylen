from collections.abc import Mapping
from types import MappingProxyType

from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.merge_mode import MergeMode
from haylen.platforms.override_section import OverrideSection
from haylen.plugins.core_contributions import CoreContributions


class AndroidContributions:
    """The contribution types that plugins give the Android project and the section `platforms.android` of `haylen.json`, with their JSON Schemas."""

    GRADLE_MODULE = "gradleModule"
    MAVEN_REPOSITORY = "mavenRepository"
    GRADLE_PLUGIN = "gradlePlugin"
    MANIFEST_PLACEHOLDER = "manifestPlaceholder"
    PROJECT_FILE = "projectFile"
    ASSET_PACKS = "assetPacks"
    SECTION = "android"
    PLATFORMS = frozenset({AndroidPlatform.PLATFORM})
    # The pattern of a name of Java and Kotlin, such as the class of the application or a package.
    QUALIFIED_NAME = "^[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_][A-Za-z0-9_]*)+$"

    @staticmethod
    def object_schema(properties: Mapping[str, object], required: tuple[str, ...]) -> dict[str, object]:
        return {"type": "object", "additionalProperties": False, "required": list(required), "properties": dict(properties)}

    @staticmethod
    def types() -> dict[str, ContributionType]:
        text = {"type": "string", "minLength": 1}
        relative = {"type": "string", "minLength": 1, "not": {"pattern": "^/|(^|/)\\.\\.(/|$)"}}
        names = {"type": "array", "items": {"type": "string", "minLength": 1}, "uniqueItems": True}
        entries = (
            (
                AndroidContributions.GRADLE_MODULE,
                {"path": relative},
                ("path",),
                (),
                MergeMode.ACCUMULATE,
                "The folder of the Android library module of the plugin, with build.gradle.kts and src/main/AndroidManifest.xml.",
            ),
            (
                AndroidContributions.MAVEN_REPOSITORY,
                {"url": {"type": "string", "pattern": '^https://[^\\s,"$]+$'}},
                ("url",),
                ("url",),
                MergeMode.UNIQUE,
                "The https address of a Maven repository that the module and its Gradle plugins come from.",
            ),
            (
                AndroidContributions.GRADLE_PLUGIN,
                {"id": {"type": "string", "pattern": "^[A-Za-z0-9_.-]+$"}, "version": {"type": "string", "pattern": "^[A-Za-z0-9_.+-]+$"}},
                ("id", "version"),
                ("id",),
                MergeMode.UNIQUE,
                "A Gradle plugin that the app module applies, by its id and version.",
            ),
            (
                AndroidContributions.MANIFEST_PLACEHOLDER,
                {"name": {"type": "string", "pattern": "^[A-Za-z][A-Za-z0-9_]*$"}, "value": {"type": ["string", "number", "boolean"]}},
                ("name", "value"),
                ("name",),
                MergeMode.UNIQUE,
                "A manifest placeholder, whose value the manifest of the module reads as ${name}.",
            ),
            (
                AndroidContributions.PROJECT_FILE,
                {"from": text, "to": relative},
                ("to",),
                ("to",),
                MergeMode.UNIQUE,
                "A copy into the Android project: a file of the plugin, or a file parameter that names a file of the app, which is left out while the parameter has no value.",
            ),
            (
                AndroidContributions.ASSET_PACKS,
                {"fastFollow": names},
                (),
                (),
                MergeMode.EXCLUSIVE,
                "Delivers the named packs of the content from Google Play, those of fastFollow right after the install and the others when the app asks for them.",
            ),
        )
        types = {
            name: ContributionType(name, AndroidContributions.PLATFORMS, MappingProxyType(AndroidContributions.object_schema(fields, required)), identity, merge, description)
            for name, fields, required, identity, merge, description in entries
        }
        types[CoreContributions.NATIVE_LIBRARY] = CoreContributions.native_library(AndroidContributions.PLATFORMS)
        return types

    @staticmethod
    def section() -> OverrideSection:
        names = {"type": "array", "items": {"type": "string", "pattern": "^[A-Za-z][A-Za-z0-9_.]*$"}, "uniqueItems": True}
        manifest = {
            "applicationAttributes": {"type": "object", "propertyNames": {"pattern": "^[a-z][A-Za-z]*$", "not": {"const": "name"}}, "additionalProperties": {"type": "string"}},
            "permissions": names,
            "removedPermissions": names,
        }
        properties = {
            "sources": {"type": "array", "items": {"type": "string", "minLength": 1}, "uniqueItems": True},
            "applicationClass": {"type": "string", "pattern": AndroidContributions.QUALIFIED_NAME},
            "manifest": AndroidContributions.object_schema(manifest, ()),
        }
        return OverrideSection(
            AndroidContributions.SECTION,
            AndroidContributions.PLATFORMS,
            MappingProxyType(AndroidContributions.object_schema(properties, ())),
            MappingProxyType({}),
            "The Android app: the Kotlin and Java folders of the app module, the class of the application, the manifest and the oldest API level.",
        )
