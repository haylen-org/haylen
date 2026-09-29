#include <gtest/gtest.h>

#include <memory>

#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Document.hpp"
#include "support/EngineFixture.hpp"
#include "ui/MountLink.hpp"

namespace haylen::ui {

TEST(MountLinkTest, UnmountsAndHidesItsDocument) {
    test::EngineFixture fixture;
    plugins::UiPlugin& plugin = fixture.engine().getPlugin<plugins::UiPlugin>();
    std::shared_ptr<Document> document = plugin.createDocument(core::Json::parse(R"({"kind": "label", "text": "hello"})"));
    plugin.mount(document);
    auto link = std::make_shared<MountLink>(plugin, document);
    core::Connection connection(link);
    EXPECT_TRUE(connection.isConnected());

    // Blocking the link hides the document without unmounting it.
    connection.setBlocked(true);
    EXPECT_TRUE(connection.isBlocked());
    EXPECT_FALSE(document->isVisible());
    connection.setBlocked(false);
    EXPECT_TRUE(document->isVisible());

    link->disconnect();
    EXPECT_FALSE(plugin.isMounted(*document));
    EXPECT_FALSE(link->isConnected());
    EXPECT_FALSE(link->isBlocked());
    link->setBlocked(true);
    EXPECT_TRUE(document->isVisible());
}

} // namespace haylen::ui
