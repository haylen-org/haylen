from haylen.core.app_category import AppCategory


class LinuxCategories:
    """The categories of a Linux app from `category` of `haylen.json`: the main categories of the freedesktop menu specification, each with an additional category where one fits, which the desktop entry and the AppStream metainfo list, and the section of the Debian archive that the `.deb` package declares. An app without a category lists none and takes the section `misc`, which Debian keeps for packages that fit no other section."""

    MENU = {
        AppCategory.GAME: ("Game",),
        AppCategory.ENTERTAINMENT: ("AudioVideo",),
        AppCategory.EDUCATION: ("Education",),
        AppCategory.PRODUCTIVITY: ("Office",),
        AppCategory.UTILITY: ("Utility",),
        AppCategory.GRAPHICS: ("Graphics",),
        AppCategory.MUSIC: ("AudioVideo", "Audio"),
        AppCategory.VIDEO: ("AudioVideo", "Video"),
        AppCategory.SOCIAL: ("Network", "Chat"),
        AppCategory.DEVELOPER: ("Development",),
    }
    UNCATEGORIZED_DEBIAN_SECTION = "misc"
    DEBIAN_SECTIONS = {
        AppCategory.GAME: "games",
        AppCategory.ENTERTAINMENT: "misc",
        AppCategory.EDUCATION: "education",
        AppCategory.PRODUCTIVITY: "misc",
        AppCategory.UTILITY: "utils",
        AppCategory.GRAPHICS: "graphics",
        AppCategory.MUSIC: "sound",
        AppCategory.VIDEO: "video",
        AppCategory.SOCIAL: "net",
        AppCategory.DEVELOPER: "devel",
    }

    @staticmethod
    def menu(category: AppCategory | None) -> tuple[str, ...]:
        return LinuxCategories.MENU[category] if category is not None else ()

    @staticmethod
    def debian_section(category: AppCategory | None) -> str:
        return LinuxCategories.DEBIAN_SECTIONS[category] if category is not None else LinuxCategories.UNCATEGORIZED_DEBIAN_SECTION
