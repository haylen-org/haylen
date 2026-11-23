import re
from pathlib import Path

from haylen.core.repository import Repository
from tests.support.engine_sources import EngineSources


class LuaLayerTest:
    """The core of the engine in `engine/include` and `engine/src` is C++ without Lua, and the Lua layer in `engine/lua` reaches it only through its public C++ API, so an app without Lua links no Lua and every capability of the engine is usable from C++."""

    LUA = re.compile(r'#\s*include\s*[<"](lua\.h|lua\.hpp|lauxlib\.h|lualib\.h|haylen/lua/|varn/lua/|varn/async/AsyncModule\.h)|\blua_State\b|\blua::|\bluaL_\w|\blua_\w')
    LUA_FILE = re.compile(r"(Scripted\w*|Lua\w*|\w+Lua)\.\w+$")
    INCLUDE = re.compile(r'#\s*include\s*"([^"]+)"')
    ENGINE_LINKS = re.compile(r"target_link_libraries\(haylen-engine\b([^)]*)\)")
    LUA_OF_VARN = re.compile(r"\bvarn-(core|vendor-lua)\b")

    @staticmethod
    def core_sources() -> dict[str, str]:
        return {name: text for name, text in EngineSources.read().items() if name.startswith(("engine/include/", "engine/src/"))}

    def test_the_core_names_nothing_of_lua(self):
        naming = sorted(name for name, text in LuaLayerTest.core_sources().items() if LuaLayerTest.LUA.search(text))
        assert naming == [], f"These files of the core include or name Lua: {naming}."

    def test_the_core_holds_no_file_of_lua(self):
        files = sorted(name for name in LuaLayerTest.core_sources() if name.startswith("engine/src/") and LuaLayerTest.LUA_FILE.match(Path(name).name))
        assert files == [], f"These files of the core belong to the Lua layer: {files}."

    def test_the_lua_layer_includes_no_private_header_of_the_core(self):
        including = []
        for name, text in EngineSources.read().items():
            if not name.startswith("engine/lua/"):
                continue
            for included in LuaLayerTest.INCLUDE.findall(text):
                if (Repository.ENGINE / "src" / included).is_file() and not (Repository.ENGINE / "lua" / "src" / included).is_file():
                    including.append(f"{name} includes {included}")
        assert including == [], f"These files of the Lua layer include private headers of the core: {sorted(including)}."

    def test_the_engine_target_links_no_lua_of_varn(self):
        cmake = (Repository.ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        links = " ".join(LuaLayerTest.ENGINE_LINKS.findall(cmake))
        assert links, "The file engine/CMakeLists.txt links no library to haylen-engine."
        assert not LuaLayerTest.LUA_OF_VARN.search(links), f"The target haylen-engine links the Lua of Varn: {links.strip()}."
