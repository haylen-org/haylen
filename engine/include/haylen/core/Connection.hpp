#pragma once

#include <memory>
#include <utility>

namespace haylen::core {

// Links a caller to something it registered: a signal slot, an event listener, a tween or a timer. The connection does not keep it alive, and every call does nothing once it is gone.
class Connection final {
  public:
    // The side of a connection that the registered thing implements. Blocking holds it without removing it: a slot or listener is skipped, and a tween or timer is paused.
    class Link {
      public:
        virtual ~Link() = default;

        virtual void disconnect() = 0;
        [[nodiscard]] virtual bool isConnected() const noexcept = 0;
        virtual void setBlocked(bool value) = 0;
        [[nodiscard]] virtual bool isBlocked() const noexcept = 0;
    };

    Connection() = default;
    explicit Connection(std::weak_ptr<Link> value) : link(std::move(value)) {}

    void disconnect() {
        if (const std::shared_ptr<Link> locked = link.lock()) {
            locked->disconnect();
        }
        link.reset();
    }

    [[nodiscard]] bool isConnected() const noexcept {
        const std::shared_ptr<Link> locked = link.lock();
        return locked && locked->isConnected();
    }

    void setBlocked(bool value) {
        if (const std::shared_ptr<Link> locked = link.lock()) {
            locked->setBlocked(value);
        }
    }

    [[nodiscard]] bool isBlocked() const noexcept {
        const std::shared_ptr<Link> locked = link.lock();
        return locked && locked->isBlocked();
    }

  private:
    std::weak_ptr<Link> link;
};

} // namespace haylen::core
