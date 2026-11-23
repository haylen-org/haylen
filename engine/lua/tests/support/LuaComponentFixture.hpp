#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "support/ComponentFixture.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::test {

// A fixture of the components of an engine that runs Lua, for the tests that drive the components from Lua.
class LuaComponentFixture : public ComponentFixture {
  protected:
    explicit LuaComponentFixture(std::map<std::string, std::string> files = {}) : ComponentFixture(std::make_unique<LuaFixture>(std::move(files))) {}

    [[nodiscard]] LuaFixture& getLuaFixture() {
        return dynamic_cast<LuaFixture&>(*fixture);
    }
};

} // namespace haylen::test
