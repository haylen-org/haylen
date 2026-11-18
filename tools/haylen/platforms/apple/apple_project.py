import json
from collections.abc import Mapping
from pathlib import Path

from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_package import ReleasePackage
from haylen.core.app_kind import AppKind
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.apple.app_extension import AppExtension
from haylen.platforms.apple.apple_contributions import AppleContributions
from haylen.platforms.apple.apple_native_libraries import AppleNativeLibraries
from haylen.platforms.apple.apple_sdk_files import AppleSdkFiles
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.info_plist_keys import InfoPlistKeys
from haylen.platforms.apple.privacy_manifest import PrivacyManifest
from haylen.platforms.apple.property_lists import PropertyLists
from haylen.platforms.apple.splash_catalog import SplashCatalog
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.product_folders import ProductFolders
from haylen.plugins.merged_contribution import MergedContribution
from haylen.toolchains.xcodegen import XcodeGen


class AppleProject:
    """Generates the Xcode project of a product into `project/` of its folders: the XcodeGen spec `project.yml` with one app target for the platform of the target and a target per app extension, the files of the template of that platform, which the `files` of the sections of `haylen.json` replace, the Info.plist, entitlements and privacy manifest joined from the template, the engine, the plugins and the app, the splash assets, the package or the protected release with its bootstrap, the native libraries and the registry of their static symbols. The app compiles the Apple sources of its plugins and the `sources` of the `apple` section, and a C++ app compiles its `cpp` sources too, against the headers of `Haylen.xcframework`, while a Lua app links the player of `HaylenPlayer.xcframework`. XcodeGen writes `App.xcodeproj` into the build folder of the product whenever the spec changes."""

    TEMPLATE = Path(__file__).resolve().parent / "template"
    XCODE_PROJECT = "App.xcodeproj"
    SPEC = "project.yml"
    CONFIG = "App.xcconfig"
    INFO_PLIST = "Info.plist"
    ENTITLEMENTS = "App.entitlements"
    PRIVACY = "PrivacyInfo.xcprivacy"
    BOOTSTRAP = ProtectedReleases.BOOTSTRAP_FILE
    PACKAGE = "app"
    TARGET = "App"
    MODULE = "HaylenApp"
    BRIDGING_HEADER = "HaylenBridging.h"
    SPLASH_LOGO = "splash-logo.svg"
    # The build settings that the backend owns, by where their values come from, which `platforms.apple.buildSettings` may not set, since another value would cancel a constraint of the SDK, the target or `haylen.json`.
    OWNED_SETTINGS = {
        "ARCHS": 'the architectures of the target, which "--architecture" chooses',
        "EXCLUDED_ARCHS": 'the architectures of the target, which "--architecture" chooses',
        "ONLY_ACTIVE_ARCH": 'the architectures of the target, which "--architecture" chooses',
        "SDKROOT": "the target",
        "SUPPORTS_MACCATALYST": "the target",
        "TARGETED_DEVICE_FAMILY": "the target",
        "OTHER_LDFLAGS": 'the engine SDK and the static native libraries of "native" and the plugins',
        "CODE_SIGN_STYLE": "the signing of the backend",
        "CODE_SIGN_IDENTITY": "the signing of the backend",
        "DEVELOPMENT_TEAM": '"platforms.apple.team" or the variable "HAYLEN_APPLE_TEAM"',
        "CODE_SIGN_ENTITLEMENTS": 'the entitlements that the backend joins from the "entitlements" of the sections and the plugins',
        "INFOPLIST_FILE": 'the Info.plist that the backend joins from the "infoPlist" of the sections and the plugins',
        "GENERATE_INFOPLIST_FILE": 'the Info.plist that the backend joins from the "infoPlist" of the sections and the plugins',
        "IPHONEOS_DEPLOYMENT_TARGET": 'the oldest system version, which "minimumSystemVersion" of the section of the platform settles',
        "MACOSX_DEPLOYMENT_TARGET": 'the oldest system version, which "minimumSystemVersion" of the section of the platform settles',
        "TVOS_DEPLOYMENT_TARGET": 'the oldest system version, which "minimumSystemVersion" of the section of the platform settles',
        "XROS_DEPLOYMENT_TARGET": 'the oldest system version, which "minimumSystemVersion" of the section of the platform settles',
        "PRODUCT_NAME": '"name" of "haylen.json"',
        "PRODUCT_BUNDLE_IDENTIFIER": '"identifier" of "haylen.json"',
        "MARKETING_VERSION": '"version" of "haylen.json"',
        "CURRENT_PROJECT_VERSION": '"build" of "haylen.json"',
    }
    # The files of the template that every target takes from the root of the template, besides the files of the folder of its platform.
    ROOT_FILES = ("main.m", BRIDGING_HEADER)
    # The bootstrap of a Debug build reads the package as it is and stops a build of the project where the preprocessor marks a release, which needs the protected release.
    DEVELOPMENT_BOOTSTRAP = "\n".join(
        [
            "// Written by the Apple backend for the Debug configuration, which ships the package of the app as it is. A Release build ships the protected release with the bootstrap of the app, which a Release preparation writes.",
            "#if !DEBUG",
            '#error "The project holds the development package of the app, which release builds never ship. Prepare the app in the \\"Release\\" configuration."',
            "#endif",
            "",
        ]
    )

    def __init__(self, context: ToolingContext, product: ResolvedProduct, target: AppleTarget, folders: ProductFolders) -> None:
        self.context = context
        self.product = product
        self.target = target
        self.folders = folders
        self.project = product.primary
        self.sdk = product.primary.sdk
        self.tree = GeneratedTree(folders.project)

    @property
    def xcode_project(self) -> Path:
        return self.folders.build / AppleProject.XCODE_PROJECT

    def in_project(self, name: str) -> str:
        """The full path of a generated file, which build settings name, since Xcode reads their paths from the folder of `App.xcodeproj` in the build folder."""
        return str(self.folders.project / name)

    def generate(self) -> GeneratedTree:
        """Writes every file of the project and returns the tree, whose `finish` the preparation calls, and generates the Xcode project when its spec changed or it is missing."""
        files = self.template_files()
        for name, path in files.items():
            if name != AppleProject.INFO_PLIST:
                self.tree.copy(name, path)
        self.write_content()
        self.write_info_plist(files[AppleProject.INFO_PLIST])
        entitlements = self.write_entitlements()
        self.write_privacy()
        if self.target.launch_screen:
            SplashCatalog.write(self.tree, self.product.app, AppleProject.TEMPLATE / AppleProject.SPLASH_LOGO)
        self.tree.copy(AppleSdkFiles.SWIFT_HELPERS_NAME, self.sdk.file(AppleSdkFiles.SWIFT_HELPERS))

        native = AppleNativeLibraries(self.context, self.product, self.target, self.folders.build).place()
        if native.registry is not None:
            self.tree.write_bytes(AppleNativeLibraries.REGISTRY, native.registry.encode("utf-8"))
        extensions = dict(AppExtension.target(self.tree, extension, self.target) for extension in self.project.of_type(AppleContributions.APP_EXTENSION))
        self.tree.write_bytes(AppleProject.CONFIG, self.xcconfig().encode("utf-8"))
        spec = self.spec(entitlements, native, extensions)
        header = f"# Written by the Apple backend for XcodeGen {XcodeGen.VERSION} from `haylen.json`, the plugins of the app and the engine SDK.\n"
        self.tree.write_bytes(AppleProject.SPEC, (header + json.dumps(spec, indent=4) + "\n").encode("utf-8"))

        if AppleProject.SPEC in self.tree.changed or not (self.xcode_project / "project.pbxproj").is_file():
            self.context.terminal.step(f"Generating `{Repository.shown(self.xcode_project)}` with XcodeGen")
            self.folders.build.mkdir(parents=True, exist_ok=True)
            command = [XcodeGen.path(self.context), "generate", "--quiet", "--spec", self.folders.project / AppleProject.SPEC, "--project", self.folders.build, "--project-root", self.folders.build]
            self.context.process.run(command, cwd=self.folders.project)
        return self.tree

    def template_files(self) -> dict[str, Path]:
        """The files of the template of the platform by their path in the project, with the `files` of the sections of `haylen.json` in place of the template files and folders they name."""
        files = {name: AppleProject.TEMPLATE / name for name in AppleProject.ROOT_FILES}
        platform = AppleProject.TEMPLATE / self.target.folder
        files |= {path.relative_to(platform).as_posix(): path for path in sorted(platform.rglob("*")) if path.is_file() and path.name != ".DS_Store"}
        if not self.target.launch_screen:
            files = {name: path for name, path in files.items() if name != "LaunchScreen.storyboard"}

        replaced: dict[str, str] = {}
        for section, settings in self.project.settings.items():
            for destination, source in Frozen.thaw(settings.get("files", {})).items():
                key = f"platforms.{section}.files.{destination}"
                if destination in replaced:
                    raise InvalidManifest(
                        f'The keys "{replaced[destination]}" and "{key}" of `{self.product.app.origin}` both replace `{destination}`. Keep one of them.', origin=self.product.app.origin
                    )
                inside = [name for name in files if name == destination or name.startswith(f"{destination}/")]
                if not inside:
                    known = ", ".join(f'"{name}"' for name in sorted({name.split("/")[0] for name in files}))
                    raise InvalidManifest(
                        f'The key "{key}" of `{self.product.app.origin}` names `{destination}`, which the template of "{self.target.name}" does not hold. It replaces {known} or a file inside them.',
                        origin=self.product.app.origin,
                    )
                replaced[destination] = key
                for name in inside:
                    del files[name]
                given = self.product.app.folder / source
                if given.is_file():
                    files[destination] = given
                    continue
                files |= {f"{destination}/{path.relative_to(given).as_posix()}": path for path in sorted(given.rglob("*")) if path.is_file() and path.name != ".DS_Store"}
        return files

    def write_content(self) -> None:
        """Writes the package of a Debug build as it is, or the part of the protected release of a Release build that ships with the app and the bootstrap that opens it."""
        if self.product.configuration is Configuration.DEBUG:
            for name, path in self.product.package.files().items():
                self.tree.copy(f"{AppleProject.PACKAGE}/{name}", path)
            self.tree.write_bytes(AppleProject.BOOTSTRAP, AppleProject.DEVELOPMENT_BOOTSTRAP.encode("utf-8"))
            return

        releases = ProtectedReleases.locate(self.context, BackendRegistry.discover(self.context))
        source = ReleasePackage.of(self.product)
        release = releases.build(source, AppleTarget.RELEASE_PROFILE, self.product.request.previous_release)
        self.tree.copy_folder(AppleProject.PACKAGE, release.shipped)
        self.tree.write_bytes(AppleProject.BOOTSTRAP, releases.bootstrap_source(source.app, AppleTarget.RELEASE_PROFILE), ProtectedReleases.BOOTSTRAP_MODE)

    @staticmethod
    def origin(contribution: MergedContribution) -> str:
        return " and ".join(f'"{owner}" at `{origin}`' for owner, origin in zip(contribution.owners, contribution.origins, strict=True))

    def contributed(self, kind: str) -> list[tuple[str, Mapping[str, object]]]:
        """The merged values of a dictionary contribution as one layer with the origins of all its owners, or no layer."""
        return [(AppleProject.origin(contribution), contribution.fields["values"]) for contribution in self.project.of_type(kind)]

    def write_info_plist(self, template: Path) -> None:
        """Writes the Info.plist of the app target: the one of the template, or the one that `files` gives, with the keys of the engine and the `infoPlist` keys of the plugins and the app."""
        base = (f"the Info.plist `{Repository.shown(template)}`", PropertyLists.read(template))
        engine = (f"the engine from `{self.product.app.origin}`", InfoPlistKeys.of(self.product.app, self.target))
        joined = PropertyLists.join([base, engine, *self.contributed(AppleContributions.INFO_PLIST)], f'the Info.plist of "{self.target.name}"')
        self.tree.write_bytes(AppleProject.INFO_PLIST, PropertyLists.dumps(joined))

    def write_entitlements(self) -> bool:
        joined = PropertyLists.join(self.contributed(AppleContributions.ENTITLEMENTS), f'the entitlements of "{self.target.name}"')
        if not joined:
            return False
        self.tree.write_bytes(AppleProject.ENTITLEMENTS, PropertyLists.dumps(joined))
        return True

    def write_privacy(self) -> None:
        engine = (f"the engine SDK `{Repository.shown(self.sdk.manifest.folder)}`", PropertyLists.read(self.sdk.file(AppleSdkFiles.PRIVACY)))
        merged = PrivacyManifest.merge([engine, *self.contributed(AppleContributions.PRIVACY)])
        self.tree.write_bytes(AppleProject.PRIVACY, PropertyLists.dumps(merged))

    def xcconfig(self) -> str:
        """The settings of the identity of the app, which every target of the project and the values of plugins name as `$(NAME)`."""
        app = self.product.app
        settings = {
            "HAYLEN_PRODUCT_NAME": app.name,
            "HAYLEN_DISPLAY_NAME": app.name,
            "HAYLEN_BUNDLE_IDENTIFIER": app.identifier,
            "MARKETING_VERSION": app.version,
            "CURRENT_PROJECT_VERSION": str(app.build),
        }
        lines = [
            "// Written by the Apple backend with the identity of the app from `haylen.json`, which every target of the project takes.",
            *(f"{name} = {value}" for name, value in settings.items()),
            "",
        ]
        return "\n".join(lines)

    @staticmethod
    def listed_path(path: Path) -> str:
        """A path in a list of build settings, which Xcode splits at every space that no quotes hold, so a folder whose name holds a space stays one path."""
        return f'"{path}"'

    def settings(self, native: AppleNativeLibraries.Placement) -> dict:
        """The build settings of the app target, with the `buildSettings` of the `apple` section of `haylen.json` last, so they win."""
        target = self.target
        architectures = " ".join(project.target.architecture for project in self.product.projects)
        sdk_settings = json.loads(self.sdk.file(AppleSdkFiles.SETTINGS).read_text(encoding="utf-8"))
        definitions = [*sdk_settings["definitions"], *(self.product.app.cpp.definitions if self.product.app.cpp else ())]
        settings: dict[str, object] = {
            "PRODUCT_NAME": "$(HAYLEN_PRODUCT_NAME)",
            "PRODUCT_BUNDLE_IDENTIFIER": "$(HAYLEN_BUNDLE_IDENTIFIER)",
            "PRODUCT_MODULE_NAME": AppleProject.MODULE,
            "INFOPLIST_FILE": self.in_project(AppleProject.INFO_PLIST),
            "GENERATE_INFOPLIST_FILE": "NO",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++20",
            "CLANG_ENABLE_OBJC_ARC": "YES",
            "DEAD_CODE_STRIPPING": "YES",
            "SWIFT_VERSION": "5.0",
            "SWIFT_OBJC_BRIDGING_HEADER": self.in_project(AppleProject.BRIDGING_HEADER),
            "ASSETCATALOG_COMPILER_APPICON_NAME": "app-icon",
            "ARCHS": architectures,
            "ONLY_ACTIVE_ARCH": "NO",
            "GCC_PREPROCESSOR_DEFINITIONS": ["$(inherited)", *definitions],
        }
        if target.device_family:
            settings["TARGETED_DEVICE_FAMILY"] = target.device_family
        if target.folder == "ios":
            settings["SUPPORTS_MACCATALYST"] = "YES" if target.name == "catalyst" else "NO"
            settings["DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER"] = "NO"
        if target.folder == "macos":
            settings["ENABLE_HARDENED_RUNTIME"] = "YES"
        if self.product.app.cpp is not None:
            settings["HEADER_SEARCH_PATHS"] = ["$(inherited)", *(AppleProject.listed_path(path) for path in self.product.app.cpp.include_directories)]
        if native.linked:
            # The static native libraries link by their paths, since they are no libraries of the search paths.
            settings["OTHER_LDFLAGS"] = ["$(inherited)", *(AppleProject.listed_path(path) for path in native.linked)]
        if self.project.of_type(AppleContributions.BUILD_SCRIPT):
            # The scripts of plugins read files of the build that the sandbox of scripts would need listed one by one.
            settings["ENABLE_USER_SCRIPT_SANDBOXING"] = "NO"

        if target.device:
            settings["CODE_SIGN_STYLE"] = "Automatic"
            settings["DEVELOPMENT_TEAM"] = self.team()
        else:
            settings["CODE_SIGN_IDENTITY"] = "-"

        given = Frozen.thaw(self.project.setting(AppleContributions.APPLE_SECTION, "buildSettings", {}))
        owned = [name for name in given if name in AppleProject.OWNED_SETTINGS]
        if owned:
            problems = "\n".join(f'  The key "platforms.apple.buildSettings.{name}" sets a value that comes from {AppleProject.OWNED_SETTINGS[name]}.' for name in owned)
            raise InvalidManifest(
                f"The file `{self.product.app.origin}` sets build settings that the Apple backend owns, so they would cancel a constraint of the SDK, the target or the app. Remove them:\n{problems}",
                origin=self.product.app.origin,
            )
        return {**settings, **given}

    def team(self) -> str:
        """The development team that signs the apps of devices: the `team` of the `apple` section of `haylen.json`, or else the team of this machine that the variable `HAYLEN_APPLE_TEAM` names, such as in continuous integration."""
        team = self.project.setting(AppleContributions.APPLE_SECTION, "team") or self.context.environment.get("HAYLEN_APPLE_TEAM")
        if not team:
            raise InvalidManifest(
                f'The apps of "{self.target.name}" are signed with a development team. Name it in "platforms.apple.team" of `{self.product.app.origin}`, or in the variable "HAYLEN_APPLE_TEAM".',
                origin=self.product.app.origin,
            )
        return str(team)

    def sources(self, entitlements: bool, registry: bool) -> list[dict]:
        sources: list[dict] = [
            {"path": self.in_project("main.m")},
            {"path": self.in_project(AppleProject.BOOTSTRAP)},
            {"path": self.in_project(AppleSdkFiles.SWIFT_HELPERS_NAME)},
            {"path": self.in_project(AppleProject.BRIDGING_HEADER)},
            {"path": self.in_project(AppleProject.INFO_PLIST), "buildPhase": "none"},
            {"path": self.in_project(AppleProject.PRIVACY), "buildPhase": "resources"},
            {"path": self.in_project("Assets.xcassets")},
            {"path": self.in_project(AppleProject.PACKAGE), "type": "folder", "buildPhase": "resources"},
        ]
        if self.target.launch_screen:
            sources += [{"path": self.in_project("LaunchScreen.storyboard")}, {"path": self.in_project(SplashCatalog.FOLDER)}]
        if entitlements:
            sources.append({"path": self.in_project(AppleProject.ENTITLEMENTS), "buildPhase": "none"})
        if registry:
            sources.append({"path": self.in_project(AppleNativeLibraries.REGISTRY)})

        for contribution in self.project.of_type(AppleContributions.SOURCES):
            sources.append(AppleProject.source(contribution.folders[0] / str(contribution.fields["path"]), "Plugins", contribution.owners[0]))
        bundled: dict[str, str] = {}
        for contribution in self.project.of_type(AppleContributions.RESOURCE):
            path = self.resource(contribution)
            owner = contribution.owners[0]
            if path.name in bundled:
                raise InvalidContribution(f'The plugins "{bundled[path.name]}" and "{owner}" both place "{path.name}" at the root of the app bundle. Keep only one of them, or rename the file in one.')
            bundled[path.name] = owner
            sources.append({"path": str(path), "buildPhase": "resources", "group": "Plugins"})
        for path in self.project.setting(AppleContributions.APPLE_SECTION, "sources", ()):
            sources.append(AppleProject.source(self.product.app.folder / str(path), "App", None))
        if self.product.app.cpp is not None:
            sources += [AppleProject.source(path, "App", None) for path in self.product.app.cpp.sources]
        return sources

    @staticmethod
    def source(path: Path, group: str, name: str | None) -> dict:
        """A folder or file of sources outside the project, which a folder that Xcode synchronizes brings with every file it holds when it builds."""
        source: dict = {"path": str(path), "group": group}
        if path.is_dir():
            source["type"] = "syncedFolder"
        if name is not None:
            source["name"] = name
        return source

    def resource(self, contribution: MergedContribution) -> Path:
        """The file of a resource of a plugin: a file of the plugin, or a file of the app that a `file` parameter of the plugin names."""
        value = str(contribution.fields["path"])
        plugin = next(plugin for plugin in self.project.plugins if plugin.id == contribution.owners[0])
        names_file = any(parameter.type == "file" and plugin.parameters.get(name) == value for name, parameter in plugin.manifest.parameters.items())
        path = (self.product.app.folder if names_file else contribution.folders[0]) / value
        if not path.exists():
            raise InvalidContribution(f'The resource of the plugin "{plugin.id}" at `{contribution.origins[0]}` names `{Repository.shown(path)}`, which does not exist.')
        return path

    def dependencies(self, native: AppleNativeLibraries.Placement, extensions: Mapping[str, dict]) -> list[dict]:
        sdk_settings = json.loads(self.sdk.file(AppleSdkFiles.SETTINGS).read_text(encoding="utf-8"))
        engine = list(sdk_settings["frameworks"][self.target.frameworks])
        dependencies: list[dict] = [{"framework": str(self.sdk.file(AppleSdkFiles.FRAMEWORK)), "embed": False}]
        if self.product.app.kind is AppKind.LUA:
            dependencies.append({"framework": str(self.sdk.file(AppleSdkFiles.PLAYER)), "embed": False})
        plugin_frameworks = [str(contribution.fields["name"]) for contribution in self.project.of_type(AppleContributions.FRAMEWORK)]
        dependencies += [{"sdk": name} for name in dict.fromkeys([*engine, *plugin_frameworks])]

        shipped: dict[str, str] = {}
        for contribution in self.project.of_type(AppleContributions.BINARY_FRAMEWORK):
            path = contribution.folders[0] / str(contribution.fields["path"])
            owner = contribution.owners[0]
            if path.name in shipped:
                raise InvalidContribution(f'The plugins "{shipped[path.name]}" and "{owner}" both ship the framework "{path.name}". Keep only one of them.')
            shipped[path.name] = owner
            dependencies.append({"framework": str(path), "embed": bool(contribution.fields["embed"])})

        for contribution in self.project.of_type(AppleContributions.SWIFT_PACKAGE):
            dependencies += [{"package": str(contribution.fields["name"]), "product": str(product)} for product in contribution.fields["products"]]
        dependencies += [{"framework": str(path), "embed": True, "link": False, "codeSign": True} for path in native.embedded]
        dependencies += [{"target": name, "embed": True} for name in extensions]
        return dependencies

    def packages(self) -> dict[str, dict]:
        """The Swift packages of the plugins by their names, which Swift Package Manager of Xcode resolves at their exact versions."""
        packages: dict[str, dict] = {}
        owners: dict[str, str] = {}
        for contribution in self.project.of_type(AppleContributions.SWIFT_PACKAGE):
            name = str(contribution.fields["name"])
            declared = {"url": str(contribution.fields["url"]), "exactVersion": str(contribution.fields["exactVersion"])}
            if name in packages and packages[name] != declared:
                raise InvalidContribution(
                    f'The plugins "{owners[name]}" and "{contribution.owners[0]}" name two Swift packages "{name}" of different addresses. Use versions of the plugins that agree on the package.'
                )
            packages[name] = declared
            owners.setdefault(name, contribution.owners[0])
        return packages

    def spec(self, entitlements: bool, native: AppleNativeLibraries.Placement, extensions: Mapping[str, dict]) -> dict:
        settings = self.settings(native)
        if entitlements:
            settings["CODE_SIGN_ENTITLEMENTS"] = self.in_project(AppleProject.ENTITLEMENTS)
        release = {
            # A release keeps its debug information in a dSYM next to the app and strips the app as it builds, so a release that runs is the one that ships.
            "DEBUG_INFORMATION_FORMAT": "dwarf-with-dsym",
            "DEPLOYMENT_POSTPROCESSING": "YES",
            "STRIP_INSTALLED_PRODUCT": "YES",
            "STRIP_STYLE": "all",
        }
        app: dict = {
            "type": "application",
            "platform": self.target.xcode_platform,
            "configFiles": {"Debug": self.in_project(AppleProject.CONFIG), "Release": self.in_project(AppleProject.CONFIG)},
            "sources": self.sources(entitlements, native.registry is not None),
            "dependencies": self.dependencies(native, extensions),
            "settings": {"base": settings, "configs": {"Release": release}},
            "scheme": {},
        }
        scripts = [Frozen.thaw(contribution.fields) for contribution in self.project.of_type(AppleContributions.BUILD_SCRIPT)]
        if scripts:
            app["postBuildScripts"] = scripts
        spec: dict = {
            "name": "App",
            "options": {"deploymentTarget": {self.target.xcode_platform: str(self.project.minimum_system_version.value)}},
            "targets": {AppleProject.TARGET: app, **extensions},
        }
        packages = self.packages()
        if packages:
            spec["packages"] = packages
        return spec
