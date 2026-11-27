import re

from haylen.core.repository import Repository
from tests.support.engine_sources import EngineSources


class LuaYieldTest:
    """A yield of Lua jumps back to its resume without running the destructors of the native frames it leaves, so every function of the engine that calls Lua through a continuation, or yields itself, has a test that yields through it, which the address sanitizer runs."""

    CALL = re.compile(r"\blua_(?:callk|pcallk|yieldk|yield)\s*\(")
    FUNCTION = re.compile(r"^[A-Za-z][\w:<>,* &]*?\b(\w+::\w+)\s*\([^;]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{", re.MULTILINE)
    TEST = re.compile(r"\bTEST(?:_F|_P)?\(\s*(\w+)\s*,\s*(\w+)\s*\)")
    # Each function that calls Lua through a continuation or yields, with the test that yields through it.
    YIELDING = {
        "ExecutionBudget::callGuarded": "ExecutionBudgetTest.KeepsTheHandlersAndYieldsOfXpcall",
        "Environment::doFileAsText": "YieldTest.PassesThroughDofile",
        "JobsBinding::checkpoint": "YieldTest.PassesThroughJobCheckpoints",
        "PlatformBinding::await": "YieldTest.PassesThroughTheAwaitOfPlatformCalls",
        "ScriptedPackSource::run": "ContentPackTest.TakesPacksFromTheSourcesThatLuaGives",
        "ScriptedPackSource::continueRun": "ContentPackTest.TakesPacksFromTheSourcesThatLuaGives",
        "ScriptedPageSource::awaitPage": "YieldTest.PassesThroughThePageLoaderOfACollection",
        "Task::body": "YieldTest.PassesThroughTasksAndThePromisesTheyReturn",
        "Task::continueBody": "YieldTest.PassesThroughTasksAndThePromisesTheyReturn",
    }

    @staticmethod
    def yielding(text: str) -> set[str]:
        """The functions of a source that call Lua through a continuation or yield, by the name of their class and their own."""
        heads = [(match.start(), match.group(1)) for match in LuaYieldTest.FUNCTION.finditer(text)]
        found = set()
        for call in LuaYieldTest.CALL.finditer(text):
            enclosing = [name for start, name in heads if start < call.start()]
            found.add(enclosing[-1] if enclosing else "")
        return found

    def test_every_function_that_yields_has_a_test_that_yields_through_it(self):
        sources = EngineSources.read()
        found = set()
        for name, text in sources.items():
            if name.startswith(("engine/lua/src/", "engine/lua/include/")):
                found |= LuaYieldTest.yielding(text)
        assert sorted(found) == sorted(LuaYieldTest.YIELDING)

        tests = {f"{suite}.{test}" for name, text in sources.items() if name.startswith("engine/lua/tests/") for suite, test in LuaYieldTest.TEST.findall(text)}
        assert sorted(set(LuaYieldTest.YIELDING.values()) - tests) == []

    def test_the_scan_names_the_function_around_a_continuation(self):
        source = (
            "int Loader::run(lua_State* L) {\n    lua_callk(L, 1, 1, 0, &finish);\n    return finish(L, LUA_OK, 0);\n}\n\nint Loader::plain(lua_State* L) {\n    lua_call(L, 0, 1);\n    return 1;\n}\n"
        )
        assert LuaYieldTest.yielding(source) == {"Loader::run"}
        assert (Repository.ENGINE / "lua" / "tests" / "runtime" / "YieldTests.cpp").is_file()
