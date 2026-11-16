from dataclasses import dataclass


@dataclass(frozen=True)
class ContentProfile:
    """The content profile that a protected release is built for, by the name that the targets of one store share, such as `apple` or `android`, with what that store bounds: the size that the shards aim for when the store limits the files that hold them, and the largest release that the store accepts inside an app with the words that name the limit. A profile that no store bounds, such as the one of a port, has neither."""

    # Google Play delivers install-time asset packs of at most 1 GB each, and a shard holds at most twice its target and one record, so the shards of Android releases always fit a pack.
    SHARD_TARGETS = {"android": 256 * 1024**2}
    # The App Store limits the uncompressed size of an iOS or tvOS app, and Google Play the compressed download of an app with its install-time asset packs, whose shards are compressed already.
    STORE_LIMITS = {
        "apple": (4 * 1024**3, "the 4 GB that the App Store accepts for the uncompressed size of an iOS or tvOS app"),
        "android": (4 * 1000**3, "the 4 GB that Google Play accepts for the download of an app with its install-time asset packs"),
    }

    name: str
    shard_target: int | None
    store_limit: int | None
    store_limit_words: str

    @staticmethod
    def of(name: str) -> ContentProfile:
        limit, words = ContentProfile.STORE_LIMITS.get(name, (None, ""))
        return ContentProfile(name, ContentProfile.SHARD_TARGETS.get(name), limit, words)
