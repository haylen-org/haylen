#pragma once

#include <memory>

#include "haylen/core/Connection.hpp"

namespace haylen::plugins {
class UiPlugin;
}

namespace haylen::ui {

class Document;

// Ties a mounted document to the owner it was mounted with: disconnecting unmounts the document and blocking hides it, and the link ends once the document is unmounted.
class MountLink final : public core::Connection::Link {
  public:
    MountLink(plugins::UiPlugin& owner, std::weak_ptr<Document> value);

    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override;
    void setBlocked(bool value) override;
    [[nodiscard]] bool isBlocked() const noexcept override;

  private:
    plugins::UiPlugin& plugin;
    std::weak_ptr<Document> document;
};

} // namespace haylen::ui
