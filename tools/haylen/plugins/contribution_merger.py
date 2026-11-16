import json
from collections.abc import Mapping, Sequence

from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.frozen import Frozen
from haylen.core.schema_validator import SchemaValidator
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.merge_mode import MergeMode
from haylen.plugins.contribution import Contribution
from haylen.plugins.merged_contribution import MergedContribution


class ContributionMerger:
    """Joins the contributions of every owner of a target by the merge mode of their type and stops at every conflict with the owners and origins on both sides, instead of letting one value replace another by the order the files were read."""

    def __init__(self, types: Mapping[str, ContributionType]) -> None:
        self.types = types

    def merge(self, contributions: Sequence[Contribution]) -> tuple[MergedContribution, ...]:
        merged: dict[tuple[str, str], MergedContribution] = {}
        conflicts: list[str] = []
        for index, contribution in enumerate(contributions):
            kind = self.types[contribution.type]
            key = (contribution.type, ContributionMerger.identity(kind, contribution, index))
            previous = merged.get(key)
            if previous is None:
                merged[key] = MergedContribution(contribution.type, contribution.fields, (contribution.owner,), (contribution.origin,), (contribution.folder,))
                continue

            if kind.merge is MergeMode.EXCLUSIVE and contribution.owner not in previous.owners:
                conflicts.append(f'The contribution "{contribution.type}" comes from one owner of an app, and both {ContributionMerger.owners(previous, contribution)} give it.')
                continue
            if kind.merge is MergeMode.DICTIONARY:
                fields = ContributionMerger.join(previous.fields, contribution.fields, contribution.type, previous, contribution, conflicts)
            elif previous.fields == contribution.fields:
                fields = previous.fields
            else:
                differing = ContributionMerger.differing(previous.fields, contribution.fields, kind.joined)
                if differing:
                    identity = ", ".join(f'"{name}" {json.dumps(Frozen.thaw(contribution.fields.get(name)))}' for name in kind.identity)
                    names = SchemaValidator.choices(differing, "and")
                    conflicts.append(f'The contribution "{contribution.type}" with {identity} differs in {names} between {ContributionMerger.owners(previous, contribution)}. Make them agree.')
                    continue
                fields = ContributionMerger.join_lists(previous.fields, contribution.fields, kind.joined)
            merged[key] = MergedContribution(
                contribution.type,
                fields,
                (*previous.owners, contribution.owner),
                (*previous.origins, contribution.origin),
                (*previous.folders, contribution.folder),
            )

        if conflicts:
            lines = "\n".join(f"  {conflict}" for conflict in conflicts)
            raise InvalidContribution(f"The contributions of the app and its plugins conflict:\n{lines}")
        return tuple(merged.values())

    @staticmethod
    def identity(kind: ContributionType, contribution: Contribution, index: int) -> str:
        if kind.merge is MergeMode.ACCUMULATE:
            return str(index)
        if kind.merge in (MergeMode.DICTIONARY, MergeMode.EXCLUSIVE):
            return ""
        return json.dumps([Frozen.thaw(contribution.fields.get(name)) for name in kind.identity])

    @staticmethod
    def differing(left: Mapping[str, object], right: Mapping[str, object], joined: tuple[str, ...]) -> list[str]:
        """The fields of two contributions of one identity that differ, apart from the lists that their type joins."""
        names = sorted({*left, *right} - set(joined))
        return [name for name in names if left.get(name) != right.get(name)]

    @staticmethod
    def join_lists(left: Mapping[str, object], right: Mapping[str, object], joined: tuple[str, ...]) -> Mapping[str, object]:
        """Joins the lists that a type joins, adding the items that the first contribution lacks, and keeps every other field, which agrees."""
        fields = dict(left)
        for name in joined:
            earlier = tuple(left.get(name, ()))
            fields[name] = (*earlier, *(item for item in right.get(name, ()) if item not in earlier))
        return Frozen.freeze(fields)

    @staticmethod
    def owners(previous: MergedContribution, contribution: Contribution) -> str:
        return f'"{" and ".join(previous.owners)}" at {", ".join(f"`{origin}`" for origin in previous.origins)} and "{contribution.owner}" at `{contribution.origin}`'

    @staticmethod
    def join(left: object, right: object, place: str, previous: MergedContribution, contribution: Contribution, conflicts: list[str]) -> object:
        """Merges two values of a dictionary contribution: objects key by key, lists by adding the items the first lacks, and other values only when they are equal."""
        if isinstance(left, Mapping) and isinstance(right, Mapping):
            joined = dict(left)
            for key, value in right.items():
                joined[key] = ContributionMerger.join(left[key], value, f"{place}.{key}", previous, contribution, conflicts) if key in left else value
            return Frozen.freeze(joined)
        if isinstance(left, tuple) and isinstance(right, tuple):
            return (*left, *(item for item in right if item not in left))
        if left != right:
            earlier = " and ".join(f'"{owner}"' for owner in previous.owners)
            conflicts.append(
                f'The key "{place}" is {json.dumps(Frozen.thaw(left))} for {earlier} and {json.dumps(Frozen.thaw(right))} for "{contribution.owner}" at `{contribution.origin}`. Make them agree.'
            )
        return left
