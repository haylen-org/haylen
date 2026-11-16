from enum import StrEnum


class MergeMode(StrEnum):
    """How the contributions of one type from several owners join for a target. `unique` keeps one contribution per identity, joins the lists that its type names and fails when two owners give the same identity different values otherwise, `accumulate` keeps every contribution, `dictionary` merges the `values` objects key by key, joining lists and failing on two different values of one key, and `exclusive` lets only one owner contribute the type."""

    UNIQUE = "unique"
    ACCUMULATE = "accumulate"
    DICTIONARY = "dictionary"
    EXCLUSIVE = "exclusive"
