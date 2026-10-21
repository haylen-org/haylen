#include <gtest/gtest.h>

#include <lua.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "support/EngineFixture.hpp"

namespace haylen::lua {

// Compares the API that Lua sees with the reference pages of the documentation in `docs/lua-api`.
class LuaReferenceTest : public ::testing::Test {
  protected:
    struct Page {
        std::string prose;
        std::vector<std::string> headings;
        std::vector<std::string> examples;
    };

    struct BoundType {
        std::set<std::string> methods;
        std::set<std::string> properties;
    };

    static std::map<std::string, Page> readPages() {
        std::map<std::string, Page> pages;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(HAYLEN_DOCS_DIR) / "lua-api")) {
            if (entry.path().extension() != ".md" || entry.path().filename() == "README.md") {
                continue;
            }
            Page& page = pages[entry.path().stem().string()];
            std::ifstream file(entry.path());
            std::string line;
            std::string example;
            bool inside = false;
            bool lua = false;
            while (std::getline(file, line)) {
                if (line.starts_with("```")) {
                    if (inside && lua) {
                        page.examples.push_back(example);
                    }
                    lua = !inside && line == "```lua";
                    inside = !inside;
                    example.clear();
                    continue;
                }
                if (inside) {
                    example += line + "\n";
                    continue;
                }
                page.prose += line + "\n";
                if (line.starts_with("#")) {
                    page.headings.push_back(line);
                }
            }
        }
        return pages;
    }

    static std::vector<std::string> getModules(lua_State* L) {
        std::vector<std::string> modules;
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "preload");
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            lua_pop(L, 1);
            const std::string name = lua_tostring(L, -1);
            if (name == "haylen" || name.starts_with("haylen.")) {
                modules.push_back(name);
            }
        }
        lua_pop(L, 2);
        std::ranges::sort(modules);
        return modules;
    }

    static std::set<std::string> getKeys(lua_State* L, int index) {
        std::set<std::string> keys;
        index = lua_absindex(L, index);
        lua_pushnil(L);
        while (lua_next(L, index) != 0) {
            lua_pop(L, 1);
            if (lua_type(L, -1) == LUA_TSTRING) {
                keys.insert(lua_tostring(L, -1));
            }
        }
        return keys;
    }

    static std::map<std::string, BoundType> getTypes(lua_State* L) {
        std::map<std::string, BoundType> types;
        lua_pushnil(L);
        while (lua_next(L, LUA_REGISTRYINDEX) != 0) {
            if (isMetatable(L)) {
                BoundType& type = types[std::string(lua_tostring(L, -2)).substr(7)];
                if (lua_getfield(L, -1, "__index") == LUA_TFUNCTION && lua_getupvalue(L, -1, 1) != nullptr) {
                    if (lua_istable(L, -1)) {
                        type.methods = getKeys(L, -1);
                    }
                    lua_pop(L, 1);
                } else if (lua_istable(L, -1)) {
                    type.methods = getKeys(L, -1);
                }
                lua_pop(L, 1);
                if (lua_getfield(L, -1, "__properties") == LUA_TTABLE) {
                    type.properties = getKeys(L, -1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        return types;
    }

    // Whether the pair on the top of the stack is a key of the registry and the metatable of a type that `ClassBuilder` named after it.
    static bool isMetatable(lua_State* L) {
        if (lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TTABLE || !std::string(lua_tostring(L, -2)).starts_with("haylen.")) {
            return false;
        }
        lua_getfield(L, -1, "__name");
        const bool named = lua_type(L, -1) == LUA_TSTRING && lua_rawequal(L, -1, -3) != 0;
        lua_pop(L, 1);
        return named;
    }

    // The name that the examples of a page give the module, such as `m` for `haylen.math`, or the last part of its name.
    static std::string getAlias(const Page& page, const std::string& module, const std::string& stem) {
        const std::regex pattern("local\\s+(\\w+)\\s*=\\s*require\\s*\\(?\\s*['\"]" + std::regex_replace(module, std::regex("\\."), "\\.") + "['\"]");
        for (const std::string& example : page.examples) {
            std::smatch match;
            if (std::regex_search(example, match, pattern)) {
                return match[1];
            }
        }
        return stem;
    }

    // Whether a heading names exactly the type, with or without backticks.
    static bool isHeadingOf(const std::string& heading, const std::string& name) {
        std::string text = heading.substr(heading.find_first_not_of('#'));
        std::erase(text, '`');
        text.erase(0, text.find_first_not_of(' '));
        return text == name;
    }

    static bool hasWord(const std::string& text, const std::string& word) {
        for (std::size_t at = text.find(word); at != std::string::npos; at = text.find(word, at + 1)) {
            const auto isName = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; };
            const bool before = at == 0 || !isName(text[at - 1]);
            const bool after = at + word.size() == text.size() || !isName(text[at + word.size()]);
            if (before && after) {
                return true;
            }
        }
        return false;
    }

    static std::string join(const std::vector<std::string>& lines) {
        std::ostringstream text;
        for (const std::string& line : lines) {
            text << "\n  " << line;
        }
        return text.str();
    }

    test::EngineFixture fixture;
    std::map<std::string, Page> pages = readPages();
};

TEST_F(LuaReferenceTest, EveryModuleAndEveryMemberHasItsPlaceInTheReference) {
    lua_State* L = fixture.lua();
    std::vector<std::string> missing;
    for (const std::string& module : getModules(L)) {
        const std::string stem = module == "haylen" ? "haylen" : module.substr(7);
        const auto page = pages.find(stem);
        if (page == pages.end()) {
            missing.push_back(module + " has no page \"docs/lua-api/" + stem + ".md\".");
            continue;
        }
        const std::string alias = getAlias(page->second, module, stem);
        lua_getglobal(L, "require");
        lua_pushstring(L, module.c_str());
        lua_call(L, 1, 1);
        if (lua_istable(L, -1)) {
            for (const std::string& key : getKeys(L, -1)) {
                if (!key.starts_with("_") && !hasWord(page->second.prose, alias + "." + key)) {
                    missing.push_back(module + "." + key);
                }
            }
        }
        lua_pop(L, 1);
    }
    EXPECT_TRUE(missing.empty()) << "These modules and members have no place in their reference page:" << join(missing);
}

TEST_F(LuaReferenceTest, EveryBoundTypeWithItsMethodsAndPropertiesHasItsPlaceInTheReference) {
    std::vector<std::string> missing;
    for (const auto& [name, type] : getTypes(fixture.lua())) {
        const auto page = std::ranges::find_if(pages, [&name](const auto& entry) { return std::ranges::any_of(entry.second.headings, [&name](const std::string& heading) { return isHeadingOf(heading, name); }); });
        if (page == pages.end()) {
            missing.push_back(name + " has no heading in the reference.");
            continue;
        }
        for (const std::string& method : type.methods) {
            if (!method.starts_with("_") && page->second.prose.find(":" + method) == std::string::npos && page->second.prose.find("." + method) == std::string::npos) {
                missing.push_back(name + ":" + method + " in " + page->first + ".md");
            }
        }
        for (const std::string& property : type.properties) {
            if (page->second.prose.find("`" + property + "`") == std::string::npos && page->second.prose.find("." + property) == std::string::npos) {
                missing.push_back(name + "." + property + " in " + page->first + ".md");
            }
        }
    }
    EXPECT_TRUE(missing.empty()) << "These types, methods and properties have no place in the reference:" << join(missing);
}

TEST_F(LuaReferenceTest, EveryLuaExampleOfTheReferenceCompiles) {
    lua_State* L = fixture.lua();
    std::vector<std::string> failing;
    for (const auto& [stem, page] : pages) {
        for (const std::string& example : page.examples) {
            if (luaL_loadbufferx(L, example.data(), example.size(), stem.c_str(), "t") != LUA_OK) {
                failing.push_back(stem + ".md: " + lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    EXPECT_TRUE(failing.empty()) << "These examples of the reference do not compile:" << join(failing);
}

} // namespace haylen::lua
