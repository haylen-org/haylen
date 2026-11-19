from enum import StrEnum


class AppCategory(StrEnum):
    """The category of an app in the stores and the app menus of the systems, which `category` of `haylen.json` names and each backend maps to the category of its platform."""

    GAME = "game"
    ENTERTAINMENT = "entertainment"
    EDUCATION = "education"
    PRODUCTIVITY = "productivity"
    UTILITY = "utility"
    GRAPHICS = "graphics"
    MUSIC = "music"
    VIDEO = "video"
    SOCIAL = "social"
    DEVELOPER = "developer"
