from haylen.core.app_category import AppCategory


class LinuxCategories:
    """The categories of a Linux app from `category` of `haylen.json`: the main categories of the freedesktop menu specification, each with an additional category where one fits, which the desktop entry and the AppStream metainfo list, and the section of the Debian archive that the `.deb` package declares."""

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
