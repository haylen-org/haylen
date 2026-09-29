#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/core/ConnectionScope.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameQueue.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::core {

TEST(SignalTest, EmitsToConnectedSlotsInOrder) {
    core::Signal<int> signal;
    std::vector<int> received;
    core::Connection first = signal.connect([&](int value) { received.push_back(value); });
    core::Connection second = signal.connect([&](int value) { received.push_back(value * 10); });

    signal.emit(2);
    EXPECT_EQ(received, (std::vector<int>{2, 20}));
    EXPECT_EQ(signal.size(), 2U);

    first.disconnect();
    EXPECT_FALSE(first.isConnected());
    signal.emit(3);
    EXPECT_EQ(received.back(), 30);
    EXPECT_EQ(signal.size(), 1U);

    signal.clear();
    EXPECT_TRUE(signal.empty());
    EXPECT_FALSE(second.isConnected());
}

TEST(SignalTest, SlotsMayDisconnectThemselvesWhileEmitting) {
    core::Signal<> signal;
    int calls = 0;
    core::Connection connection;
    // clang-format off
    connection = signal.connect([&] {
        ++calls;
        connection.disconnect();
    });
    // clang-format on

    signal.emit();
    signal.emit();
    EXPECT_EQ(calls, 1);
}

TEST(SignalTest, ScopedConnectionDisconnectsOnDestruction) {
    core::Signal<> signal;
    int calls = 0;
    {
        core::ScopedConnection scoped = signal.connect([&] { ++calls; });
        core::ScopedConnection moved = std::move(scoped);
        EXPECT_TRUE(moved.isConnected());
        EXPECT_FALSE(scoped.isConnected());
        signal.emit();
    }
    signal.emit();
    EXPECT_EQ(calls, 1);

    core::ScopedConnection reassigned = signal.connect([&] { ++calls; });
    reassigned = signal.connect([&] { calls += 10; });
    signal.emit();
    EXPECT_EQ(calls, 11);
    reassigned.disconnect();
    EXPECT_TRUE(signal.empty());
}

TEST(SignalTest, ConnectionsEndWithAClearOrTheSignal) {
    core::Connection cleared;
    core::Connection destroyed;
    {
        core::Signal<> signal;
        cleared = signal.connect([] {});
        destroyed = signal.connect([] {});
        signal.clear();
        EXPECT_FALSE(cleared.isConnected());
        destroyed = signal.connect([] {});
        EXPECT_TRUE(destroyed.isConnected());
    }
    EXPECT_FALSE(destroyed.isConnected());
}

TEST(SignalTest, SkipsSlotsDisconnectedDuringAnEmit) {
    core::Signal<> signal;
    std::vector<std::string> calls;
    core::Connection second;
    // clang-format off
    signal.connect([&] {
        calls.emplace_back("first");
        second.disconnect();
    });
    // clang-format on
    second = signal.connect([&] { calls.emplace_back("second"); });
    signal.emit();
    EXPECT_EQ(calls, std::vector<std::string>{"first"});
}

TEST(SignalTest, ConnectionOutlivingSignalIsSafe) {
    core::Connection connection;
    {
        core::Signal<> signal;
        connection = signal.connect([] {});
    }
    connection.disconnect();
    EXPECT_FALSE(connection.isConnected());
}

TEST(SignalTest, OrdersSlotsByPriority) {
    core::Signal<> signal;
    std::vector<std::string> calls;
    signal.connect([&] { calls.emplace_back("normal"); });
    signal.connect([&] { calls.emplace_back("late"); }, {.priority = -5});
    signal.connect([&] { calls.emplace_back("early"); }, {.priority = 10});
    signal.connect([&] { calls.emplace_back("normal second"); });
    signal.emit();
    EXPECT_EQ(calls, (std::vector<std::string>{"early", "normal", "normal second", "late"}));
}

TEST(SignalTest, SlotsConnectedWhileEmittingWaitForTheNextEmit) {
    core::Signal<> signal;
    std::vector<std::string> calls;
    // clang-format off
    signal.connect([&] {
        calls.emplace_back("outer");
        if (calls.size() == 1) {
            signal.connect([&] { calls.emplace_back("inner"); }, {.priority = 100});
        }
    });
    // clang-format on
    signal.emit();
    EXPECT_EQ(calls, std::vector<std::string>{"outer"});
    signal.emit();
    EXPECT_EQ(calls, (std::vector<std::string>{"outer", "inner", "outer"}));
    EXPECT_EQ(signal.size(), 2U);
}

TEST(SignalTest, OnceSlotsRunASingleTime) {
    core::Signal<int> signal;
    int total = 0;
    const core::Connection once = signal.connect([&](int value) { total += value; }, {.once = true});
    signal.emit(3);
    signal.emit(4);
    EXPECT_EQ(total, 3);
    EXPECT_FALSE(once.isConnected());
    EXPECT_TRUE(signal.empty());
}

TEST(SignalTest, BlocksTheSignalOrOneSlot) {
    core::Signal<> signal;
    int first = 0;
    int second = 0;
    core::Connection connection = signal.connect([&] { ++first; });
    signal.connect([&] { ++second; });

    connection.setBlocked(true);
    EXPECT_TRUE(connection.isBlocked());
    signal.emit();
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);

    connection.setBlocked(false);
    signal.setBlocked(true);
    EXPECT_TRUE(signal.isBlocked());
    signal.emit();
    EXPECT_EQ(first + second, 1);
    EXPECT_EQ(signal.getEmissionCount(), 2U);

    signal.setBlocked(false);
    signal.emit();
    EXPECT_EQ(first + second, 3);
}

TEST(SignalTest, SlotsOfADestroyedOwnerDisconnect) {
    core::Signal<> signal;
    int calls = 0;
    auto owner = std::make_shared<int>(0);
    const core::Connection connection = signal.connect([&] { ++calls; }, {.owner = owner});
    signal.emit();
    owner.reset();
    EXPECT_EQ(signal.getStaleCount(), 1U);
    signal.emit();
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(connection.isConnected());
    EXPECT_EQ(signal.getStaleCount(), 0U);
}

TEST(SignalTest, DeferredSlotsRunWhenTheQueueFlushes) {
    core::FrameQueue queue;
    core::Signal<const std::string&, int> signal;
    std::vector<std::string> received;
    signal.connect([&](const std::string& text, int number) { received.push_back(text + std::to_string(number)); }, {.queue = &queue});
    core::Connection dropped = signal.connect([&](const std::string&, int) { received.emplace_back("dropped"); }, {.queue = &queue});

    std::string text = "a";
    signal.emit(text, 1);
    text = "changed";
    dropped.disconnect();
    EXPECT_TRUE(received.empty());
    EXPECT_EQ(queue.size(), 2U);

    queue.flush();
    EXPECT_EQ(received, std::vector<std::string>{"a1"});

    // A deferred once slot still runs its single call after the emit disconnected it.
    received.clear();
    signal.connect([&](const std::string& value, int number) { received.push_back("once " + value + std::to_string(number)); }, {.once = true, .queue = &queue});
    signal.emit("b", 2);
    signal.emit("c", 3);
    queue.flush();
    EXPECT_EQ(received, (std::vector<std::string>{"b2", "once b2", "c3"}));

    core::Signal<int&> mutableSignal;
    EXPECT_THROW((void)mutableSignal.connect([](int&) {}, {.queue = &queue}), std::logic_error);
    EXPECT_THROW((void)signal.connect({}), std::invalid_argument);
}

TEST(SignalTest, ErrorsLeaveTheSignalUsable) {
    core::Signal<> signal;
    int calls = 0;
    signal.connect([] { throw std::runtime_error("slot failed"); });
    signal.connect([&] { ++calls; }, {.priority = -1});
    EXPECT_THROW(signal.emit(), std::runtime_error);
    signal.connect([&] { calls += 10; }, {.priority = 5});
    EXPECT_THROW(signal.emit(), std::runtime_error);
    EXPECT_EQ(calls, 10);
    EXPECT_EQ(signal.size(), 3U);
}

TEST(ConnectionScopeTest, EndsEveryConnectionItHolds) {
    core::Signal<> signal;
    int calls = 0;
    core::ConnectionScope scope;
    scope.add(signal.connect([&] { ++calls; }));
    scope.add(signal.connect([&] { ++calls; }));
    signal.emit();
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(scope.size(), 2U);

    scope.clear();
    EXPECT_TRUE(scope.empty());
    signal.emit();
    EXPECT_EQ(calls, 2);

    // A scope keeps working after a clear, and a moved scope takes its connections along.
    scope.add(signal.connect([&] { ++calls; }));
    core::ConnectionScope moved = std::move(scope);
    signal.emit();
    EXPECT_EQ(calls, 3);
    moved = core::ConnectionScope();
    signal.emit();
    EXPECT_EQ(calls, 3);
}

TEST(ConnectionScopeTest, DropsFinishedConnectionsAsItGrows) {
    core::Signal<> signal;
    core::ConnectionScope scope;
    for (int index = 0; index < 200; ++index) {
        core::Connection connection = signal.connect([] {}, {.once = true});
        scope.add(connection);
        signal.emit();
    }
    EXPECT_LT(scope.size(), 100U);
}

TEST(FrameQueueTest, RunsCallsPostedFromAnyThreadOnFlush) {
    core::FrameQueue queue;
    std::vector<int> order;
    queue.post([&] { order.push_back(1); });
    std::thread worker([&queue, &order] { queue.post([&order] { order.push_back(2); }); });
    worker.join();
    // clang-format off
    queue.post([&] {
        order.push_back(3);
        queue.post([&] { order.push_back(4); });
    });
    // clang-format on

    queue.flush();
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(queue.size(), 1U);
    queue.flush();
    EXPECT_EQ(order.back(), 4);

    queue.post([] { throw std::runtime_error("first"); });
    queue.post([&] { order.push_back(5); });
    EXPECT_THROW(queue.flush(), std::runtime_error);
    EXPECT_EQ(order.back(), 5);

    queue.post([&] { order.push_back(6); });
    queue.clear();
    queue.flush();
    EXPECT_EQ(order.back(), 5);
}

TEST(EventBusTest, DeliversNamedEventsByPriorityAndChannel) {
    core::FrameQueue queue;
    core::EventBus bus(queue);
    std::vector<std::string> calls;
    bus.on("damage", [&](core::EventBus::Event& event) { calls.push_back("any " + std::string(event.getChannel()) + " " + std::to_string(event.getData().value("amount", 0))); });
    bus.on("damage", [&](core::EventBus::Event&) { calls.emplace_back("player"); }, {.channel = "player", .priority = 5});
    bus.on("heal", [&](core::EventBus::Event&) { calls.emplace_back("heal"); });

    EXPECT_FALSE(bus.emit("damage", {{"amount", 3}}, "player"));
    EXPECT_FALSE(bus.emit("damage", {{"amount", 1}}, "enemy"));
    EXPECT_FALSE(bus.emit("unknown"));
    EXPECT_EQ(calls, (std::vector<std::string>{"player", "any player 3", "any enemy 1"}));
}

TEST(EventBusTest, ListenersConsumeFilterAndRunOnce) {
    core::FrameQueue queue;
    core::EventBus bus(queue);
    std::vector<std::string> calls;
    // clang-format off
    bus.on("click", [&](core::EventBus::Event& event) {
        calls.emplace_back("top");
        if (event.getData().value("consume", false)) {
            event.consume();
        }
    }, {.priority = 10});
    // clang-format on
    bus.on("click", [&](core::EventBus::Event&) { calls.emplace_back("bottom"); });
    bus.on("click", [&](core::EventBus::Event&) { calls.emplace_back("filtered"); }, {.once = true, .filter = [](const core::EventBus::Event& event) { return event.getData().value("x", 0) > 5; }});

    EXPECT_TRUE(bus.emit("click", {{"consume", true}}));
    EXPECT_FALSE(bus.emit("click", {{"x", 1}}));
    EXPECT_FALSE(bus.emit("click", {{"x", 9}}));
    EXPECT_FALSE(bus.emit("click", {{"x", 9}}));
    EXPECT_EQ(calls, (std::vector<std::string>{"top", "top", "bottom", "top", "bottom", "filtered", "top", "bottom"}));
}

TEST(EventBusTest, TypedEventsAndNativeValues) {
    struct Scored {
        int points = 0;
    };
    core::FrameQueue queue;
    core::EventBus bus(queue);
    int total = 0;
    bus.on<Scored>([&](core::EventBus::Event& event) { total += event.get<Scored>()->points; });
    // clang-format off
    bus.on("bonus", [&](core::EventBus::Event& event) {
        EXPECT_EQ(event.get<int>(), nullptr);
        total += event.get<Scored>()->points * 100;
    });
    // clang-format on

    EXPECT_FALSE(bus.emit(Scored{.points = 2}));
    EXPECT_FALSE(bus.emitWith("bonus", Scored{.points = 1}));
    EXPECT_EQ(total, 102);
}

TEST(EventBusTest, PostedEventsArriveWhenTheQueueFlushes) {
    struct Loaded {
        std::string name;
    };
    core::FrameQueue queue;
    core::EventBus bus(queue);
    std::vector<std::string> received;
    bus.on("saved", [&](core::EventBus::Event& event) { received.push_back("saved " + event.getData().get<std::string>()); });
    bus.on<Loaded>([&](core::EventBus::Event& event) { received.push_back("loaded " + event.get<Loaded>()->name); });

    // clang-format off
    std::thread worker([&bus] {
        bus.post("saved", "slot1");
        bus.post(Loaded{.name = "level"});
    });
    // clang-format on
    worker.join();
    EXPECT_TRUE(received.empty());
    queue.flush();
    EXPECT_EQ(received, (std::vector<std::string>{"saved slot1", "loaded level"}));
}

TEST(EventBusTest, OwnersClearAndStatistics) {
    core::FrameQueue queue;
    core::EventBus bus(queue);
    int calls = 0;
    auto owner = std::make_shared<int>(0);
    bus.on("tick", [&](core::EventBus::Event&) { ++calls; }, {.owner = owner});
    const core::Connection kept = bus.on("tick", [&](core::EventBus::Event&) { calls += 10; });
    bus.emit("tick");
    owner.reset();

    const std::vector<core::EventBus::Topic> topics = bus.getTopics();
    ASSERT_EQ(topics.size(), 1U);
    EXPECT_EQ(topics[0].name, "tick");
    EXPECT_EQ(topics[0].listeners, 2U);
    EXPECT_EQ(topics[0].emissions, 1U);
    EXPECT_EQ(topics[0].stale, 1U);

    bus.emit("tick");
    EXPECT_EQ(calls, 21);
    bus.clear();
    bus.emit("tick");
    EXPECT_EQ(calls, 21);
    EXPECT_FALSE(kept.isConnected());
    EXPECT_THROW((void)bus.on("tick", {}), std::invalid_argument);
}

} // namespace haylen::core
