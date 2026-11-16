import json
import re
from collections.abc import Mapping

from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_entry import ReleaseEntry
from haylen.content.release_package import ReleasePackage
from haylen.core.configuration import Configuration
from haylen.core.frozen import Frozen
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.apple.app_bundle import AppBundle
from haylen.platforms.apple.apple_contributions import AppleContributions
from haylen.platforms.apple.apple_sdk_files import AppleSdkFiles
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.info_plist_keys import InfoPlistKeys
from haylen.platforms.apple.property_lists import PropertyLists
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.verification_report import VerificationReport


class AppleVerifier:
    """Examines a built Apple app for what the engine and the plugins of its target need: the architectures of its code, the system frameworks it links, the Objective-C classes of the plugins, the Info.plist keys, the entitlements it is signed with, the privacy declarations and the resources of the plugins, and in Release that it ships only the protected release of the app."""

    SOURCE = "apple"
    # The settings of the identity of an app that values of plugins name as `$(NAME)`, which Xcode replaces when it builds the Info.plist and the entitlements.
    SETTING = re.compile(r"\$\((\w+)\)")

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @staticmethod
    def expand(value: object, settings: Mapping[str, str]) -> object:
        if isinstance(value, str):
            return AppleVerifier.SETTING.sub(lambda match: settings.get(match[1], match[0]), value)
        if isinstance(value, list):
            return [AppleVerifier.expand(item, settings) for item in value]
        if isinstance(value, dict):
            return {key: AppleVerifier.expand(item, settings) for key, item in value.items()}
        return value

    def verify(self, product: BuildProduct, target: AppleTarget) -> VerificationReport:
        resolved = product.prepared.product
        project = resolved.primary
        app = resolved.app
        bundle = AppBundle(self.context, product.path, target.runs_on_mac)
        settings = {
            "HAYLEN_PRODUCT_NAME": app.name,
            "HAYLEN_DISPLAY_NAME": app.name,
            "HAYLEN_BUNDLE_IDENTIFIER": app.identifier,
            "MARKETING_VERSION": app.version,
            "CURRENT_PROJECT_VERSION": str(app.build),
        }
        checks: list[VerificationCheck] = []

        wanted = {project_target.target.architecture for project_target in resolved.projects}
        built = bundle.architectures()
        missing = sorted(wanted - built)
        checks.append(self.check("architectures", not missing, f"The app holds the code of {', '.join(sorted(built))}.", f"The app lacks the code of {', '.join(missing)}, which the product names."))

        sdk_settings = json.loads(project.sdk.file(AppleSdkFiles.SETTINGS).read_text(encoding="utf-8"))
        frameworks = {name: "the engine" for name in sdk_settings["frameworks"][target.frameworks]}
        frameworks |= {str(contribution.fields["name"]): f'the plugin "{contribution.owners[0]}"' for contribution in project.of_type(AppleContributions.FRAMEWORK)}
        linked = bundle.linked()
        absent = [f'"{name}" of {owner}' for name, owner in frameworks.items() if name not in linked]
        checks.append(self.check("frameworks", not absent, "The app links every system framework of the engine and its plugins.", f"The app does not link {', '.join(absent)}."))

        classes = bundle.classes()
        lost = [f'"{contribution.fields["name"]}" of the plugin "{contribution.owners[0]}"' for contribution in project.of_type(AppleContributions.CLASS) if contribution.fields["name"] not in classes]
        checks.append(self.check("classes", not lost, "The app holds the class of every plugin.", f"The app lacks the classes {', '.join(lost)}, so those plugins do not load."))

        needed = InfoPlistKeys.of(app, target)
        if target.folder != "macos":
            needed["UIApplicationSceneManifest"] = {}
        for contribution in project.of_type(AppleContributions.INFO_PLIST):
            needed |= Frozen.thaw(contribution.fields["values"])
        checks.append(self.holds("infoPlist", bundle.info, needed, settings, "the Info.plist"))
        signed = bundle.entitlements()
        for contribution in project.of_type(AppleContributions.ENTITLEMENTS):
            checks.append(self.holds("entitlements", signed, Frozen.thaw(contribution.fields["values"]), settings, "the entitlements it is signed with"))
        privacy = bundle.privacy()
        checks.append(self.holds("privacy", privacy, PropertyLists.read(project.sdk.file(AppleSdkFiles.PRIVACY)), settings, "the privacy manifest"))
        for contribution in project.of_type(AppleContributions.PRIVACY):
            checks.append(self.holds("privacy", privacy, Frozen.thaw(contribution.fields["values"]), settings, "the privacy manifest"))

        absent_resources = [str(contribution.fields["path"]).rsplit("/", 1)[-1] for contribution in project.of_type(AppleContributions.RESOURCE)]
        absent_resources = [name for name in absent_resources if not (bundle.resources / name).exists()]
        checks.append(self.check("resources", not absent_resources, "The app holds the resources of its plugins.", f"The app lacks the resources {', '.join(absent_resources)} of its plugins."))

        if resolved.configuration is Configuration.RELEASE:
            releases = ProtectedReleases.locate(self.context, BackendRegistry.discover(self.context))
            folder = bundle.package.relative_to(product.path).as_posix()
            checks += releases.inspection(ReleasePackage.of(resolved)).checks(ReleaseEntry.of_folder(product.path), folder)
        return VerificationReport(tuple(checks))

    @staticmethod
    def check(name: str, passed: bool, success: str, failure: str) -> VerificationCheck:
        if passed:
            return VerificationCheck.passed(name, success, AppleVerifier.SOURCE)
        return VerificationCheck.failed(name, f"{failure} Prepare and build the app again with the backend of its target.", AppleVerifier.SOURCE)

    def holds(self, name: str, built: Mapping, needed: Mapping, settings: Mapping[str, str], where: str) -> VerificationCheck:
        expanded = AppleVerifier.expand(needed, settings)
        lacking = [key for key, value in expanded.items() if not PropertyLists.holds(built, {key: value})]
        return AppleVerifier.check(name, not lacking, f"The app holds the keys of {where} that it needs.", f"The app lacks {', '.join(f'"{key}"' for key in lacking)} in {where}.")
