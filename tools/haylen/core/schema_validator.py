import json
from collections.abc import Iterable, Mapping
from importlib import resources

from jsonschema import Draft202012Validator, ValidationError

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen


class SchemaValidator:
    """Checks a JSON document against a JSON Schema and describes every problem as a sentence that names the key and what it takes, so a manifest fails with all its problems at once."""

    RANGE_WORDS = {"minimum": "at least", "exclusiveMinimum": "more than", "maximum": "at most", "exclusiveMaximum": "less than"}
    TYPE_WORDS = {
        "object": "an object",
        "array": "a list",
        "string": "a text",
        "integer": "a whole number",
        "number": "a number",
        "boolean": "true or false",
        "null": "null",
    }
    # The schemas ship with the tooling like its code, so a process reads each one once and keeps validating with it while the files change on disk.
    LOADED: dict[str, SchemaValidator] = {}

    def __init__(self, schema: Mapping[str, object]) -> None:
        plain = Frozen.thaw(schema)
        Draft202012Validator.check_schema(plain)
        self.schema = plain
        self.validator = Draft202012Validator(plain)

    @staticmethod
    def of(name: str) -> SchemaValidator:
        """Returns the validator of a schema of the tooling, such as `haylen.schema.json`, which the process reads the first time it is asked for."""
        loaded = SchemaValidator.LOADED.get(name)
        if loaded is None:
            text = resources.files("haylen.schemas").joinpath(name).read_text(encoding="utf-8")
            loaded = SchemaValidator(json.loads(text))
            SchemaValidator.LOADED[name] = loaded
        return loaded

    def problems(self, document: object) -> list[str]:
        errors = sorted(self.validator.iter_errors(document), key=lambda error: [str(part) for part in error.absolute_path])
        sentences = [SchemaValidator.sentence(error) for error in errors]
        return list(dict.fromkeys(sentences))

    def require(self, document: object, origin: str) -> None:
        """Fails with every problem of a document, which `origin` names, such as the path of `haylen.json`."""
        problems = self.problems(document)
        if problems:
            lines = "\n".join(f"  {problem}" for problem in problems)
            raise InvalidManifest(f"The file `{origin}` has these problems:\n{lines}", origin=origin)

    @staticmethod
    def key(path: Iterable[object]) -> str:
        """Names a place of a document like the docs do, such as `content.delivery[2].mode`."""
        text = ""
        for part in path:
            if isinstance(part, int):
                text += f"[{part}]"
            else:
                text += f".{part}" if text else str(part)
        return text

    @staticmethod
    def sentence(error: ValidationError) -> str:
        place = SchemaValidator.key(error.absolute_path)
        subject = f'The key "{place}"' if place else "The file"
        kind = error.validator
        instance = error.instance

        if kind == "additionalProperties":
            known = sorted(error.schema.get("properties", {}))
            unknown = ", ".join(f'"{name}"' for name in sorted(set(instance) - set(known)))
            return f"{subject} has the unknown keys {unknown}. It takes {SchemaValidator.choices(known)}." if known else f"{subject} takes no keys {unknown}."
        if kind == "required":
            missing = [name for name in error.validator_value if name not in instance]
            properties = error.schema.get("properties", {})
            described = "".join(f" {properties[name]['description']}" for name in missing if "description" in properties.get(name, {}))
            return f"{subject} needs {SchemaValidator.choices(missing, 'and')}.{described}"
        if kind == "type":
            return f"{subject} must be {SchemaValidator.type_words(error.validator_value)}."
        if kind == "enum":
            return f"{subject} has the unknown value {json.dumps(instance)}. It is {SchemaValidator.choices(error.validator_value)}."
        if kind == "const":
            return f"{subject} must be {json.dumps(error.validator_value)}, not {json.dumps(instance)}."
        if kind == "propertyNames":
            return f"{subject} has the key {json.dumps(instance)}, which does not have the form of its names."
        if kind in SchemaValidator.RANGE_WORDS:
            return f"{subject} is {json.dumps(instance)}, and it must be {SchemaValidator.RANGE_WORDS[kind]} {error.validator_value}."
        if kind in {"minItems", "minProperties", "minLength"}:
            return f"{subject} must not be empty."
        if kind == "uniqueItems":
            return f"{subject} names an item more than once."
        if kind == "dependentRequired":
            needed = [item for name, items in error.validator_value.items() if name in instance for item in items if item not in instance]
            return f"{subject} needs {SchemaValidator.choices(needed, 'and')} for the keys it has."

        description = error.schema.get("description")
        detail = f" {description}" if description else ""
        if kind == "pattern":
            return f"{subject} has the value {json.dumps(instance)}, which does not have the form the key takes.{detail}"
        return f"{subject} does not have a form that it takes.{detail}"

    @staticmethod
    def type_words(kind: object) -> str:
        if isinstance(kind, list):
            return " or ".join(SchemaValidator.TYPE_WORDS[name] for name in kind)
        return SchemaValidator.TYPE_WORDS[str(kind)]

    @staticmethod
    def choices(values: Iterable[object], joiner: str = "or") -> str:
        quoted = [f'"{value}"' if isinstance(value, str) else json.dumps(value) for value in values]
        if len(quoted) == 1:
            return quoted[0]
        return f"{', '.join(quoted[:-1])} {joiner} {quoted[-1]}"
