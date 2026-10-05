#include <gtest/gtest.h>

#include <memory>

#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "support/EngineFixture.hpp"
#include "ui/MountLink.hpp"

namespace haylen::ui {

TEST(MountLinkTest, UnmountsAndHidesItsGui) {
    test::EngineFixture fixture;
    plugins::UiPlugin& plugin = fixture.engine().getPlugin<plugins::UiPlugin>();
    std::shared_ptr<Gui> gui = plugin.createGui(core::Json::parse(R"({"kind": "label", "text": "hello"})"));
    plugin.mount(gui);
    auto link = std::make_shared<MountLink>(plugin, gui);
    core::Connection connection(link);
    EXPECT_TRUE(connection.isConnected());

    // Blocking the link hides the GUI without unmounting it.
    connection.setBlocked(true);
    EXPECT_TRUE(connection.isBlocked());
    EXPECT_FALSE(gui->isVisible());
    connection.setBlocked(false);
    EXPECT_TRUE(gui->isVisible());

    link->disconnect();
    EXPECT_FALSE(plugin.isMounted(*gui));
    EXPECT_FALSE(link->isConnected());
    EXPECT_FALSE(link->isBlocked());
    link->setBlocked(true);
    EXPECT_TRUE(gui->isVisible());
}

} // namespace haylen::ui
