import re

from haylen.core.repository import Repository
from tests.support.engine_sources import EngineSources


class AndroidKeepRulesTest:
    """The Android runtime finds the Java classes and static methods of the `haylen` library by name from `JNI_OnLoad`, which fails the start of the app when one is missing, so the R8 rules that the library hands to apps keep every class and method that the C++ of the runtime looks up, as it looks them up."""

    FOLDER = "engine/src/platform/android/"
    RULES = Repository.ENGINE / "platform" / "android" / "haylen" / "consumer-rules.pro"
    ENGINE_PACKAGE = "dev/haylen/"
    # The helpers of `JavaBridge` through which the runtime looks up the classes and methods of the library.
    CLASS_LOOKUP = re.compile(r'(\w+)\s*=\s*findClass\(\*env,\s*"([^"]+)"\)')
    METHOD_LOOKUP = re.compile(r'findMethod\(\*env,\s*(\w+),\s*"([^"]+)",\s*"([^"]+)"\)')
    # A lookup of JNI with a literal name, which must name no class of the library outside the helpers.
    RAW_LOOKUP = re.compile(r'\b(FindClass|GetMethodID|GetStaticMethodID|GetFieldID|GetStaticFieldID)\s*\([^;]*"' + ENGINE_PACKAGE)
    KEEP = re.compile(r"-keep class ([\w.$]+) \{([^}]*)\}")
    TYPES = {"V": "void", "Z": "boolean", "B": "byte", "C": "char", "S": "short", "I": "int", "J": "long", "F": "float", "D": "double"}

    @staticmethod
    def java_type(descriptor: str, start: int) -> tuple[str, int]:
        """The Java type of the descriptor at a place, as R8 rules write it, and the place after it."""
        dimensions = 0
        while descriptor[start] == "[":
            dimensions += 1
            start += 1
        if descriptor[start] == "L":
            end = descriptor.index(";", start)
            name = descriptor[start + 1 : end].replace("/", ".")
            start = end + 1
        else:
            name = AndroidKeepRulesTest.TYPES[descriptor[start]]
            start += 1
        return name + "[]" * dimensions, start

    @staticmethod
    def member(name: str, descriptor: str) -> str:
        """The static method of a name and a JNI descriptor as an R8 rule writes it, such as `static byte[][] launchArguments()`."""
        arguments = []
        place = 1
        while descriptor[place] != ")":
            argument, place = AndroidKeepRulesTest.java_type(descriptor, place)
            arguments.append(argument)
        result, _ = AndroidKeepRulesTest.java_type(descriptor, place + 1)
        return f"static {result} {name}({', '.join(arguments)})"

    @staticmethod
    def lookups() -> dict[str, set[str]]:
        """The methods that the runtime looks up in each class of the library, by the binary name of the class."""
        sources = {name: text for name, text in EngineSources.read().items() if name.startswith(AndroidKeepRulesTest.FOLDER)}
        found: dict[str, set[str]] = {}
        for text in sources.values():
            classes = dict(AndroidKeepRulesTest.CLASS_LOOKUP.findall(text))
            for name in classes.values():
                if name.startswith(AndroidKeepRulesTest.ENGINE_PACKAGE):
                    found.setdefault(name, set())
            for owner, method, descriptor in AndroidKeepRulesTest.METHOD_LOOKUP.findall(text):
                found[classes[owner]].add(AndroidKeepRulesTest.member(method, descriptor))
        return found

    @staticmethod
    def kept() -> dict[str, set[str]]:
        """The members that the rules keep in each class by its binary name, with `*` for a class kept whole."""
        rules = AndroidKeepRulesTest.RULES.read_text(encoding="utf-8")
        kept: dict[str, set[str]] = {}
        for name, body in AndroidKeepRulesTest.KEEP.findall(rules):
            members = {" ".join(line.strip().rstrip(";").split()) for line in body.splitlines() if line.strip()}
            kept.setdefault(name.replace(".", "/"), set()).update(members)
        return kept

    def test_the_rules_of_r8_keep_every_class_and_method_that_the_runtime_looks_up_by_name(self):
        lookups = AndroidKeepRulesTest.lookups()
        kept = AndroidKeepRulesTest.kept()

        assert "dev/haylen/HaylenActivity" in lookups
        missing_classes = sorted(name for name in lookups if name not in kept)
        missing_methods = sorted(f"{name}: {member}" for name, members in lookups.items() if name in kept and "*" not in kept[name] for member in members - kept[name])
        assert missing_classes == []
        assert missing_methods == []

    def test_the_runtime_looks_up_the_classes_of_the_library_only_through_its_helpers(self):
        sources = {name: text for name, text in EngineSources.read().items() if name.startswith(AndroidKeepRulesTest.FOLDER)}
        raw = sorted(name for name, text in sources.items() if AndroidKeepRulesTest.RAW_LOOKUP.search(text))
        assert raw == []
