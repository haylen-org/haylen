#include "haylen/debug/ObjectCounter.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>

namespace haylen::debug {

struct ObjectCounter::Registry {
    std::mutex mutex;
    std::vector<ObjectCounter*> counters;
    std::shared_ptr<const Observer> observer;
};

std::atomic<bool> ObjectCounter::observed{false};

// The registry is never destroyed, so it outlives every counter, and an `exit()` on another thread, which runs the destructors of static objects, leaves it to the threads that still count.
ObjectCounter::Registry& ObjectCounter::getRegistry() {
    static Registry& registry = *new Registry();
    return registry;
}

// The registry keeps its counters in order of name, so a reader lists them without sorting.
ObjectCounter::ObjectCounter(std::string_view counterName, Kind counterKind) : name(counterName), kind(counterKind) {
    Registry& registry = getRegistry();
    const std::scoped_lock lock(registry.mutex);
    const auto place = std::ranges::upper_bound(registry.counters, name, {}, &ObjectCounter::name);
    registry.counters.insert(place, this);
}

ObjectCounter::~ObjectCounter() {
    Registry& registry = getRegistry();
    const std::scoped_lock lock(registry.mutex);
    std::erase(registry.counters, this);
}

void ObjectCounter::add(std::size_t count) noexcept {
    created.fetch_add(count, std::memory_order_relaxed);

    if (isObserved()) {
        notify(true, count);
    }
}

void ObjectCounter::remove(std::size_t count) noexcept {
    destroyed.fetch_add(count, std::memory_order_relaxed);

    if (isObserved()) {
        notify(false, count);
    }
}

void ObjectCounter::addBytes(std::int64_t delta) noexcept {
    bytes.fetch_add(delta, std::memory_order_relaxed);
}

ObjectCounter::Snapshot ObjectCounter::getSnapshot() const {
    Snapshot snapshot;
    readSnapshot(snapshot);
    return snapshot;
}

void ObjectCounter::readSnapshot(Snapshot& snapshot) const {
    const std::uint64_t made = created.load(std::memory_order_relaxed);
    const std::uint64_t gone = destroyed.load(std::memory_order_relaxed);
    snapshot.name = name;
    snapshot.kind = kind;
    snapshot.created = made;
    snapshot.destroyed = gone;
    snapshot.alive = made > gone ? made - gone : 0;
    snapshot.bytes = bytes.load(std::memory_order_relaxed);
}

void ObjectCounter::readAll(std::vector<Snapshot>& snapshots) {
    Registry& registry = getRegistry();
    const std::scoped_lock lock(registry.mutex);
    snapshots.resize(registry.counters.size());
    for (std::size_t index = 0; index < registry.counters.size(); ++index) {
        registry.counters[index]->readSnapshot(snapshots[index]);
    }
}

std::optional<ObjectCounter::Snapshot> ObjectCounter::find(std::string_view counterName) {
    Registry& registry = getRegistry();
    const std::scoped_lock lock(registry.mutex);
    const auto found = std::ranges::find(registry.counters, counterName, &ObjectCounter::name);
    return found != registry.counters.end() ? std::optional<Snapshot>((*found)->getSnapshot()) : std::nullopt;
}

void ObjectCounter::setObserver(Observer value) {
    Registry& registry = getRegistry();
    const std::scoped_lock lock(registry.mutex);
    registry.observer = value ? std::make_shared<const Observer>(std::move(value)) : nullptr;
    observed.store(registry.observer != nullptr, std::memory_order_relaxed);
}

// The observer runs outside the lock, so it may create and destroy counted objects itself.
void ObjectCounter::notify(bool wasCreated, std::size_t count) const {
    std::shared_ptr<const Observer> observer;
    {
        Registry& registry = getRegistry();
        const std::scoped_lock lock(registry.mutex);
        observer = registry.observer;
    }

    if (observer) {
        (*observer)(name, wasCreated, count);
    }
}

} // namespace haylen::debug
