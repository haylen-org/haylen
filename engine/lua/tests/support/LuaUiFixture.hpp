#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "support/LuaFixture.hpp"
#include "support/UiFixture.hpp"

namespace haylen::test {

// A fixture of the GUIs of an engine that runs Lua, for the tests that drive the GUIs from Lua.
class LuaUiFixture : public UiFixture {
  public:
    explicit LuaUiFixture(std::map<std::string, std::string> files = {}) : UiFixture(std::make_unique<LuaFixture>(std::move(files))) {}
    LuaUiFixture(std::map<std::string, std::string> files, EngineFixture::Options options) : UiFixture(std::make_unique<LuaFixture>(std::move(files), nullptr, std::move(options))) {}

    [[nodiscard]] LuaFixture& getFixture() {
        return dynamic_cast<LuaFixture&>(UiFixture::getFixture());
    }
};

} // namespace haylen::test
