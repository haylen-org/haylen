from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import PurePosixPath

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.frozen import Frozen
from haylen.core.package_layout import PackageLayout
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.resolved_project import ResolvedProject
from haylen.core.resolved_value import ResolvedValue
from haylen.core.schema_validator import SchemaValidator
from haylen.core.target_id import TargetId
from haylen.core.version import Version
from haylen.native.native_library import NativeLibrary
from haylen.native.static_linking import StaticLinking
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.override_section import OverrideSection
from haylen.platforms.target_description import TargetDescription
from haylen.plugins.contribution import Contribution
from haylen.plugins.contribution_merger import ContributionMerger
from haylen.plugins.core_contributions import CoreContributions
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.sdk.sdk_selector import SdkSelector


@dataclass(frozen=True)
class ProjectResolver:
    """Turns a request into a resolved product in one pipeline, before any backend runs: it picks the target from the descriptions of the registry, resolves the plugins for its platform, merges the contributions of the app and the plugins, settles the oldest system version, chooses the SDK slice of every architecture and freezes the result. The core never names a platform, so a new backend joins without a change here."""

    # The keys that the core reads in every section of `platforms`, apart from the settings that the backend of the section reads.
    CORE_KEYS = ("minimumSystemVersion", "architectures", "files", "afterPrepare")
    # The keys that belong to one platform, so only the section named after a platform takes them.
    PLATFORM_KEYS = ("minimumSystemVersion", "architectures")

    registry: BackendRegistry
    plugins: PluginResolver
    sdks: SdkSelector

    # Why a Release build needs the identifier that `haylen.json` names.
    RELEASE_IDENTIFIER = "A Release build ships the protected release of the app, whose content keys are kept per identifier"

    def resolve(self, request: ProductRequest) -> ResolvedProduct:
        app = self.plugins.app
        if request.configuration is Configuration.RELEASE:
            app.require_identifier(ProjectResolver.RELEASE_IDENTIFIER)
        target = self.registry.target(request.target)
        description = target.description
        if app.kind not in description.kinds:
            raise UnsupportedTarget(f'The target "{description.name}" does not build apps of the kind "{app.kind.value}", and `{app.origin}` describes one.', origin=app.origin)
        sections = self.sections(app, description.platform)
        target.backend.check_app(app, description.name)
        target_ids = ProjectResolver.target_ids(description, request, app)
        loaded = self.plugins.load(request.configuration)
        types = ProjectResolver.contribution_types(self.registry, description.platform)
        resolved = self.plugins.for_platform(loaded, description.platform, types)
        overrides = ProjectResolver.override_contributions(app, sections, self.registry.override_sections(), description.platform)
        contributions = ContributionMerger(types).merge((*resolved.contributions, *overrides))
        minimum = ProjectResolver.minimum_system_version(app, sections, resolved.contributions, description.platform)
        graphics = tuple(app.section("window").get("graphicsApis", ()))
        native = ProjectResolver.native_libraries(app, resolved.contributions, description.platform)
        package = PackageLayout.of_app(app, {plugin.id: plugin.folder for plugin in resolved.plugins})
        permissions = Frozen.freeze({permission: plugin.id for plugin in resolved.plugins for permission in plugin.manifest.permissions})
        settings, defaulted = ProjectResolver.settings(sections, self.registry.override_sections(), description.platform)

        projects = []
        for target_id in target_ids:
            floor = str(minimum.value) if minimum.value is not None else None
            sdk = self.sdks.select(target_id, description.name, request.engine_configuration, floor, graphics)
            effective = minimum
            if minimum.value is None or Version.parse(sdk.slice.minimum_system_version) > Version.parse(str(minimum.value)):
                effective = ResolvedValue(sdk.slice.minimum_system_version, f"the SDK `{Repository.shown(sdk.manifest.folder)}`", "The SDK runs on no older system.")
            missing = sorted(description.requires - sdk.slice.capabilities)
            if missing:
                raise UnsupportedTarget(f'The SDK of "{description.name}" lacks the capabilities {SchemaValidator.choices(missing, "and")} that its backend needs.')
            StaticLinking.check(native, description.platform, description.name, sdk.slice.capabilities)
            projects.append(
                ResolvedProject(
                    app=app,
                    package=package,
                    target=target_id,
                    target_name=description.name,
                    platform=description.platform,
                    engine_configuration=request.engine_configuration,
                    sdk=sdk,
                    plugins=resolved.plugins,
                    contributions=contributions,
                    settings=settings,
                    defaulted_settings=defaulted,
                    native_libraries=native,
                    permissions=permissions,
                    minimum_system_version=effective,
                )
            )

        architectures = ProjectResolver.architectures(description, request, app)
        identity = f"{description.name}-{request.configuration.value.lower()}"
        if tuple(architectures) != description.default_architectures:
            identity += "-" + "-".join(architectures)
        return ResolvedProduct(request, target.platform.name, identity, tuple(projects))

    @staticmethod
    def architectures(description: TargetDescription, request: ProductRequest, app: AppManifest) -> tuple[str, ...]:
        """Returns the architectures that a request builds: the ones it names, or else the `architectures` of the section of the platform in `haylen.json`, or else the default architectures of the target. A request that names an architecture that the app leaves out fails with both owners."""
        named = app.platforms.get(description.platform, {}).get("architectures")
        if named is None:
            return tuple(request.architectures or description.default_architectures)
        key = f"platforms.{description.platform}.architectures"
        unknown = [architecture for architecture in named if architecture not in description.abis]
        if unknown:
            raise InvalidManifest(
                f'The key "{key}" of `{app.origin}` names {SchemaValidator.choices(unknown, "and")}, while "{description.name}" builds for {SchemaValidator.choices(description.architectures)}.',
                origin=app.origin,
            )
        left_out = [architecture for architecture in request.architectures or () if architecture not in named]
        if left_out:
            raise UnsupportedTarget(
                f'The option "--architecture" names {SchemaValidator.choices(left_out, "and")}, which "{key}" of `{app.origin}` leaves out. Add it to the key, or name one of {SchemaValidator.choices(list(named))}.',
                origin=app.origin,
            )
        return tuple(request.architectures or named)

    @staticmethod
    def target_ids(description: TargetDescription, request: ProductRequest, app: AppManifest) -> tuple[TargetId, ...]:
        """Returns what the request builds on each of its architectures, and fails on an architecture that the target does not build for."""
        architectures = ProjectResolver.architectures(description, request, app)
        unknown = [architecture for architecture in architectures if architecture not in description.abis]
        if unknown:
            raise UnsupportedTarget(f'The target "{description.name}" builds for {SchemaValidator.choices(description.architectures)}, not for {SchemaValidator.choices(unknown, "and")}.')
        return tuple(
            TargetId(description.system, architecture, description.abis[architecture], description.environment, request.configuration, description.toolchain) for architecture in architectures
        )

    @staticmethod
    def contribution_types(registry: BackendRegistry, platform: str) -> Mapping[str, ContributionType]:
        """Returns the contribution types that plugins may give for a platform: the ones its backends declare and the oldest system version of the core."""
        types = dict(registry.contribution_types(platform))
        types[CoreContributions.MINIMUM_SYSTEM_VERSION] = CoreContributions.minimum_system_version(frozenset({platform}))
        return types

    def sections(self, app: AppManifest, platform: str) -> dict[str, Mapping[str, object]]:
        """Checks every section of `platforms` against the backend that reads it and returns the sections that apply to a platform."""
        known = self.registry.override_sections()
        problems: list[str] = []
        applying: dict[str, Mapping[str, object]] = {}
        for name, section in app.platforms.items():
            override = known.get(name)
            if override is None:
                names = SchemaValidator.choices(sorted(known)) if known else "none"
                problems.append(f'The section "platforms.{name}" is read by no installed backend. The sections are {names}.')
                continue
            problems += ProjectResolver.section_problems(app, name, section, override, known)
            if platform in override.platforms:
                applying[name] = section
        if problems:
            lines = "\n".join(f"  {problem}" for problem in problems)
            raise InvalidManifest(f"The file `{app.origin}` has these problems:\n{lines}", origin=app.origin)
        return applying

    @staticmethod
    def section_problems(app: AppManifest, name: str, section: Mapping[str, object], override: OverrideSection, known: Mapping[str, OverrideSection]) -> list[str]:
        problems: list[str] = []
        own = {key: value for key, value in section.items() if key not in ProjectResolver.CORE_KEYS}
        for problem in SchemaValidator(override.schema).problems(Frozen.thaw(own)):
            problems.append(f'In "platforms.{name}": {problem}')

        minimum = section.get("minimumSystemVersion")
        if minimum is not None and not (isinstance(minimum, str) and Version.is_valid(minimum)):
            problems.append(f'The key "platforms.{name}.minimumSystemVersion" must be a version such as "16.3".')
        architectures = section.get("architectures")
        listed = isinstance(architectures, list | tuple) and len(architectures) > 0 and all(isinstance(item, str) for item in architectures)
        if architectures is not None and not (listed and len(set(architectures)) == len(architectures)):
            problems.append(f'The key "platforms.{name}.architectures" must list one or more architectures of the platform, each once, such as ["arm64-v8a"].')
        # The oldest system version and the architectures belong to one platform, so only the section named after a platform takes them.
        for key in ProjectResolver.PLATFORM_KEYS:
            if key in section and name not in override.platforms:
                takers = [f"platforms.{platform}" for platform in sorted(override.platforms) if platform in known]
                sections = f"The sections {SchemaValidator.choices(takers, 'and')} take it." if takers else "No section of its platforms takes it."
                problems.append(f'The section "platforms.{name}" is named after no platform, so it takes no "{key}", which belongs to one platform. {sections}')
        script = section.get("afterPrepare")
        if script is not None and not (isinstance(script, str) and script.endswith(".py") and (app.folder / script).is_file()):
            problems.append(f'The key "platforms.{name}.afterPrepare" must name a Python script of the app folder that exists, such as "native/after-prepare.py".')
        files = section.get("files", {})
        if not isinstance(files, Mapping):
            problems.append(f'The key "platforms.{name}.files" must map paths of the generated project to files or folders of the app.')
            return problems
        for destination, source in files.items():
            path = PurePosixPath(destination)
            if path.is_absolute() or ".." in path.parts or not isinstance(source, str) or not (app.folder / source).exists():
                problems.append(f'The key "platforms.{name}.files.{destination}" must map a path inside the generated project to a file or folder of the app folder that exists.')
        return problems

    @staticmethod
    def settings(sections: Mapping[str, Mapping[str, object]], known: Mapping[str, OverrideSection], platform: str) -> tuple[Mapping[str, Mapping[str, object]], Mapping[str, frozenset[str]]]:
        """Returns the settings of every section that applies to a platform, the ones of the app over the defaults of the backend, with the keys of each section that come from its defaults."""
        settings: dict[str, dict[str, object]] = {}
        defaulted: dict[str, frozenset[str]] = {}
        for name, override in known.items():
            if platform not in override.platforms:
                continue
            given = {key: value for key, value in sections.get(name, {}).items() if key != "minimumSystemVersion"}
            defaults = {key: value for key, value in override.defaults.items() if key not in given}
            if name in sections or defaults:
                settings[name] = {**defaults, **given}
            if defaults:
                defaulted[name] = frozenset(defaults)
        return Frozen.freeze(settings), Frozen.freeze(defaulted)

    @staticmethod
    def override_contributions(app: AppManifest, sections: Mapping[str, Mapping[str, object]], known: Mapping[str, OverrideSection], platform: str) -> tuple[Contribution, ...]:
        """Turns the keys of the sections that are contributions of the app into contributions owned by `app`, which merge with those of the plugins."""
        contributions = []
        for name, section in sections.items():
            for key, kind in known[name].contribution_keys.items():
                if key in section:
                    origin = f"{app.origin} at platforms.{name}.{key}"
                    contributions.append(Contribution(kind, (platform,), Frozen.freeze({"values": section[key]}), "app", origin, app.folder))
        return tuple(contributions)

    @staticmethod
    def minimum_system_version(app: AppManifest, sections: Mapping[str, Mapping[str, object]], contributions: tuple[Contribution, ...], platform: str) -> ResolvedValue:
        """Settles the oldest system version of a platform. A version of the app is final, and a plugin that needs a newer one fails the resolution instead of moving the audience of the app, while without one the newest version that a plugin needs counts."""
        requirements = [(Version.parse(str(item.fields["version"])), item) for item in contributions if item.type == CoreContributions.MINIMUM_SYSTEM_VERSION]
        own = sections.get(platform, {}).get("minimumSystemVersion")
        if own is not None:
            version = Version.parse(str(own))
            higher = [item for needed, item in requirements if needed > version]
            if higher:
                needs = [f'the plugin "{item.owner}" needs {item.fields["version"]}' for item in higher]
                joined = needs[0] if len(needs) == 1 else f"{', '.join(needs[:-1])} and {needs[-1]}"
                raise UnsupportedTarget(
                    f'The app runs on "{platform}" {own} and newer, as "platforms.{platform}.minimumSystemVersion" of `{app.origin}` says, while {joined}. Raise the version of the app, or remove the plugin.',
                    origin=app.origin,
                )
            return ResolvedValue(str(own), f'{app.origin} at "platforms.{platform}.minimumSystemVersion"', "The app sets it.")
        if not requirements:
            return ResolvedValue(None, "the SDK", "Neither the app nor a plugin sets it.")
        version, item = max(requirements, key=lambda requirement: requirement[0])
        return ResolvedValue(str(item.fields["version"]), item.origin, f'The plugin "{item.owner}" needs it, and the app sets none.')

    @staticmethod
    def native_libraries(app: AppManifest, contributions: tuple[Contribution, ...], platform: str) -> tuple[NativeLibrary, ...]:
        """Returns the native libraries of the app and of its plugins that ship to a platform. The library of a plugin takes the id of the plugin as its name."""
        libraries = [library for library in app.native if library.ships_to(platform)]
        for contribution in contributions:
            if contribution.type != CoreContributions.NATIVE_LIBRARY:
                continue
            entry = {**Frozen.thaw(contribution.fields), "platforms": [platform]}
            if any(library.name == contribution.owner for library in libraries):
                raise InvalidManifest(f'The plugin "{contribution.owner}" adds the native library "{contribution.owner}", which `{app.origin}` already names. Rename the library of the app.')
            libraries.append(NativeLibrary.parse(contribution.owner, entry, contribution.folder, contribution.owner, contribution.origin))
        return tuple(libraries)
