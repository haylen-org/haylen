from collections.abc import Mapping, Sequence

from haylen.platforms.apple.property_lists import PropertyLists


class PrivacyManifest:
    """Joins the privacy manifests of the engine, the plugins and the app into the `PrivacyInfo.xcprivacy` of an app. The reasons of each accessed API type join into one entry of that type, as the App Store expects one entry per type, and every other key joins like any property list, so two owners that disagree on tracking stop the preparation with both named."""

    ACCESSED = "NSPrivacyAccessedAPITypes"
    TYPE = "NSPrivacyAccessedAPIType"
    REASONS = "NSPrivacyAccessedAPITypeReasons"

    @staticmethod
    def merge(layers: Sequence[tuple[str, Mapping[str, object]]]) -> dict:
        accessed: dict[str, list[str]] = {}
        others = []
        for origin, manifest in layers:
            for entry in manifest.get(PrivacyManifest.ACCESSED, ()):
                reasons = accessed.setdefault(str(entry[PrivacyManifest.TYPE]), [])
                reasons += [str(reason) for reason in entry[PrivacyManifest.REASONS] if reason not in reasons]
            others.append((origin, {key: value for key, value in manifest.items() if key != PrivacyManifest.ACCESSED}))

        merged = PropertyLists.join(others, "the privacy manifest")
        if accessed:
            merged[PrivacyManifest.ACCESSED] = [{PrivacyManifest.TYPE: kind, PrivacyManifest.REASONS: reasons} for kind, reasons in accessed.items()]
        return merged
