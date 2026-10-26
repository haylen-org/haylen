#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/core/JsonBytes.hpp"
#include "haylen/platform/Bridge.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/GamepadSlots.hpp"
#include "platform/GamepadVendors.hpp"
#include "platform/KeyboardTranslator.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/ReportedState.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(KeyboardTranslatorTest, TypesWhatThePlainKeyboardCommits) {
    KeyboardTranslator keyboard;
    // clang-format off
    const auto describe = [&keyboard](const Event& event) {
        std::string typed;
        for (const Event& key : keyboard.translate(event)) {
            if (key.type == Event::Type::Character) {
                typed += static_cast<char>(key.character);
            } else if (key.type == Event::Type::KeyDown) {
                typed += "<" + std::to_string(static_cast<int>(key.key)) + ">";
            }
        }
        return typed;
    };
    const auto edit = [](std::string text, int compositionStart = -1, int compositionEnd = -1) {
        return Event{.type = Event::Type::TextEdited, .textEdit = {.text = std::move(text), .compositionStart = compositionStart, .compositionEnd = compositionEnd}};
    };
    // clang-format on
    const std::string backspace = "<" + std::to_string(static_cast<int>(input::Key::Backspace)) + ">";
    const std::string enter = "<" + std::to_string(static_cast<int>(input::Key::Enter)) + ">";

    EXPECT_EQ(describe(edit("hi")), "hi");
    EXPECT_EQ(describe(edit("hi th", 3, 5)), " ") << "Text inside an open composition waits.";
    EXPECT_EQ(describe(edit("hi there")), "there");
    EXPECT_EQ(describe(edit("hi their")), backspace + backspace + "ir") << "A corrected word is erased and typed again.";
    EXPECT_EQ(describe(edit("hi their\n")), enter);
    EXPECT_EQ(describe(Event{.type = Event::Type::TextAction, .textAction = TextInput::Action::Submit}), enter);
    EXPECT_EQ(describe(Event{.type = Event::Type::TextAction, .textAction = TextInput::Action::Dismissed}), "");
    for (const Event& typed : keyboard.translate(edit("hi their!"))) {
        EXPECT_TRUE(typed.typedOnScreen);
    }

    keyboard.reset();
    EXPECT_EQ(describe(edit("new")), "new");
}

TEST(BridgeRelayTest, ForwardsNativeRepliesToTheAttachedBridge) {
    std::vector<std::string> calls;
    Bridge bridge([&calls](std::uint64_t, std::string_view method, std::string_view, std::span<const std::vector<std::byte>>) { calls.emplace_back(method); }, [](std::uint64_t, std::string_view) {});
    std::vector<Bridge::Result> results;
    std::vector<core::Json> events;
    const std::uint64_t call = bridge.call("native.method", {.json = core::Json::object()}, [&](Bridge::Result result) { results.push_back(std::move(result)); });
    core::Connection connection = bridge.on("native.event", [&](const Bridge::Payload& payload) { events.push_back(payload.json); });

    BridgeRelay::resolve(call, true, "1");
    BridgeRelay::emit("native.event", "2", {}, {});
    BridgeRelay::emit("native.late", "5", {}, {.retain = true});
    bridge.pump();
    EXPECT_TRUE(results.empty()) << "Nothing reaches a bridge that is not attached.";

    BridgeRelay::attach(bridge);
    BridgeRelay::resolve(call, true, R"({"value": 3, "raw": {"$bytes": 0}})", {{std::byte{9}, std::byte{0}}});
    BridgeRelay::emit("native.event", "4", {}, {});
    BridgeRelay::emit("native.late", "6", {}, {.retain = true});
    BridgeRelay::detach(bridge);
    bridge.pump();
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results.front().value.json.at("value"), 3);
    EXPECT_EQ(results.front().value.buffers, (std::vector<std::vector<std::byte>>{{std::byte{9}, std::byte{0}}}));
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events.front(), 4);
    EXPECT_EQ(calls, (std::vector<std::string>{"native.method"}));

    // The relay keeps the retain flag, so the event that came while the bridge was attached waits for its first listener.
    core::Connection late = bridge.on("native.late", [&](const Bridge::Payload& payload) { events.push_back(payload.json); });
    bridge.pump();
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(events.back(), 6);
}

TEST(MemoryWarningTest, HandsEachWarningOverOnce) {
    EXPECT_FALSE(MemoryWarning::take());
    MemoryWarning::raise();
    MemoryWarning::raise();
    EXPECT_TRUE(MemoryWarning::take());
    EXPECT_FALSE(MemoryWarning::take());
}

TEST(ReportedStateTest, GivesANewAppTheStateThePlatformReportedLast) {
    ReportedState state;
    EXPECT_TRUE(state.getReplay().empty());
    for (const Event::Type type : {Event::Type::FocusLost, Event::Type::InterruptionBegan, Event::Type::Suspended}) {
        EXPECT_TRUE(state.record({.type = type}));
    }
    EXPECT_TRUE(state.record({.type = Event::Type::NetworkChanged, .online = false}));
    std::vector<Event::Type> replayed;
    for (const Event& event : state.getReplay()) {
        replayed.push_back(event.type);
    }
    EXPECT_EQ(replayed, (std::vector{Event::Type::NetworkChanged, Event::Type::FocusLost, Event::Type::InterruptionBegan, Event::Type::Suspended}));

    for (const Event::Type type : {Event::Type::FocusGained, Event::Type::InterruptionEnded, Event::Type::Resumed}) {
        EXPECT_TRUE(state.record({.type = type}));
    }
    ASSERT_EQ(state.getReplay().size(), 1U);
    EXPECT_FALSE(state.getReplay()[0].online);
}

TEST(ReportedStateTest, KeepsAPausedAppSuspendedWhileItsPageShowsAndHides) {
    ReportedState state;
    EXPECT_EQ(state.setPaused(true)->type, Event::Type::Suspended);
    EXPECT_FALSE(state.record({.type = Event::Type::Suspended}));
    EXPECT_FALSE(state.record({.type = Event::Type::Resumed})) << "A paused app stays suspended when its page shows again.";
    EXPECT_FALSE(state.setPaused(true).has_value());

    EXPECT_FALSE(state.record({.type = Event::Type::Suspended}));
    EXPECT_FALSE(state.setPaused(false).has_value()) << "An app resumed in a hidden page waits until the page shows.";
    EXPECT_EQ(state.getReplay().back().type, Event::Type::Suspended);
    EXPECT_TRUE(state.record({.type = Event::Type::Resumed}));
}

TEST(GamepadSlotsTest, KeepsEveryControllerInItsSlotWhileItStaysConnected) {
    std::array<int, input::Input::kMaxGamepads + 2> pads{};
    const auto pad = [&pads](std::size_t index) -> const void* { return &pads[index]; };
    GamepadSlots slots;

    slots.update(std::vector{pad(0), pad(1), pad(2)});
    EXPECT_EQ(slots.getController(0), pad(0));
    EXPECT_EQ(slots.getController(1), pad(1));
    EXPECT_EQ(slots.getController(2), pad(2));

    // The first controller leaves, and the platform lists the others in a new order.
    slots.update(std::vector{pad(2), pad(1)});
    EXPECT_EQ(slots.getController(0), nullptr);
    EXPECT_EQ(slots.getController(1), pad(1));
    EXPECT_EQ(slots.getController(2), pad(2));

    // New controllers fill the free slots, and the one beyond the last slot waits for a slot to free up.
    std::vector<const void*> listed{pad(1), pad(2)};
    for (std::size_t index = 3; index < pads.size(); ++index) {
        listed.push_back(pad(index));
    }
    slots.update(listed);
    EXPECT_EQ(slots.getController(0), pad(3));
    EXPECT_EQ(slots.getController(3), pad(4));
    EXPECT_EQ(slots.getController(input::Input::kMaxGamepads - 1), pad(input::Input::kMaxGamepads));

    // A controller that takes the place of one that left waits an update, so the app hears the one that left first.
    listed.erase(listed.begin());
    slots.update(listed);
    EXPECT_EQ(slots.getController(1), nullptr);
    slots.update(listed);
    EXPECT_EQ(slots.getController(1), pad(input::Input::kMaxGamepads + 1));
    EXPECT_EQ(slots.getController(input::Input::kMaxGamepads), nullptr);

    slots.update({});
    for (std::size_t slot = 0; slot < input::Input::kMaxGamepads; ++slot) {
        EXPECT_EQ(slots.getController(slot), nullptr);
    }
}

TEST(GamepadVendorsTest, LabelsTheFaceButtonsByTheVendorOfTheGamepad) {
    EXPECT_EQ(GamepadVendors::getLabels(0x045E), input::GamepadLabels::Letters);
    EXPECT_EQ(GamepadVendors::getLabels(0x054C), input::GamepadLabels::Shapes);
    EXPECT_EQ(GamepadVendors::getLabels(0x057E), input::GamepadLabels::SwappedLetters);
    EXPECT_EQ(GamepadVendors::getLabels(0x1234), input::GamepadLabels::Unknown);
}

TEST(BridgeTest, KeepsTheMessageCodeAndDataOfEveryFailure) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
    std::vector<Bridge::Result> results;
    std::vector<std::uint64_t> calls;
    for (int index = 0; index < 9; ++index) {
        calls.push_back(bridge.call("native.method", {.json = core::Json::object()}, [&results](Bridge::Result result) { results.push_back(std::move(result)); }));
    }
    const std::vector<std::string> payloads{"null", "42", "[1, 2]", R"({"code": 3})", R"({"message": 3})", "", R"("denied")", R"({"message": "no network", "code": "offline", "data": {"retry": 5}})", "{broken"};
    for (std::size_t index = 0; index < payloads.size(); ++index) {
        bridge.resolve(calls[index], false, payloads[index]);
    }
    bridge.pump();

    ASSERT_EQ(results.size(), payloads.size());
    for (std::size_t index = 0; index < 6; ++index) {
        EXPECT_FALSE(results[index].ok);
        EXPECT_EQ(results[index].error.message, "The native platform call failed without a message.") << payloads[index];
    }
    EXPECT_EQ(results[3].error.code, 3);
    EXPECT_EQ(results[6].error.message, "denied");
    EXPECT_TRUE(results[6].error.code.is_null());
    EXPECT_EQ(results[7].error.message, "no network");
    EXPECT_EQ(results[7].error.code, "offline");
    EXPECT_EQ(results[7].error.data.at("retry"), 5);
    EXPECT_EQ(results[8].error.message, "The platform returned invalid JSON.");
    EXPECT_EQ(results[8].error.code, "invalidJson");
}

TEST(BridgeTest, TimesOutAndCancelsCallsAndTellsNativeCode) {
    std::vector<std::pair<std::uint64_t, std::string>> cancelled;
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [&cancelled](std::uint64_t id, std::string_view method) { cancelled.emplace_back(id, method); });
    Bridge::Reply late;
    bridge.registerHandler("engine.slow", [&late](const Bridge::Payload&, Bridge::Reply reply) { late = std::move(reply); });

    std::vector<std::string> codes;
    const auto record = [&codes](Bridge::Result result) { codes.push_back(result.ok ? "ok" : result.error.code.get<std::string>()); };
    const std::uint64_t quick = bridge.call("native.quick", {}, record, std::chrono::milliseconds(1));
    const std::uint64_t answered = bridge.call("native.answered", {}, record, std::chrono::hours(1));
    const std::uint64_t dropped = bridge.call("native.dropped", {}, record);
    bridge.call("engine.slow", {}, record, std::chrono::milliseconds(1));
    bridge.resolve(answered, true, "1");
    EXPECT_TRUE(bridge.cancel(dropped));
    EXPECT_FALSE(bridge.cancel(dropped));
    EXPECT_EQ(bridge.getPendingCallCount(), 4U);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    bridge.resolve(dropped, true, "2");
    bridge.pump();
    late({.ok = true});
    bridge.pump();

    // Engine handlers have nothing native to tell, and answers that come after the call gave up are dropped.
    EXPECT_EQ(codes, (std::vector<std::string>{"ok", "cancelled", "timeout", "timeout"}));
    EXPECT_EQ(cancelled, (std::vector<std::pair<std::uint64_t, std::string>>{{dropped, "native.dropped"}, {quick, "native.quick"}}));
    EXPECT_EQ(bridge.getPendingCallCount(), 0U);
    EXPECT_FALSE(bridge.cancel(answered));
}

// A bridge that goes away, as the one of an app that restarts does, tells native code of every call it still waited for, so native work such as a watch that streams events stops with it.
TEST(BridgeTest, CancelsThePendingNativeCallsWhenItGoesAway) {
    std::vector<std::pair<std::uint64_t, std::string>> cancelled;
    std::uint64_t watch = 0;
    {
        Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [&cancelled](std::uint64_t id, std::string_view method) { cancelled.emplace_back(id, method); });
        bridge.registerHandler("engine.slow", [](const Bridge::Payload&, Bridge::Reply) {});
        watch = bridge.call("native.watch", {}, [](const Bridge::Result&) {});
        bridge.call("engine.slow", {}, [](const Bridge::Result&) {});
        const std::uint64_t answered = bridge.call("native.answered", {}, [](const Bridge::Result&) {});
        bridge.resolve(answered, true, "1");
        bridge.pump();
    }
    EXPECT_EQ(cancelled, (std::vector<std::pair<std::uint64_t, std::string>>{{watch, "native.watch"}}));
}

TEST(BridgeTest, RunsPostedWorkOnTheFrameThreadUntilTheBridgeIsGone) {
    std::vector<std::thread::id> threads;
    // clang-format off
    Bridge::Mailbox kept = [&threads] {
        Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
        Bridge::Mailbox mailbox = bridge.getMailbox();
        std::thread worker([&] { EXPECT_TRUE(mailbox.post([&threads] { threads.push_back(std::this_thread::get_id()); })); });
        worker.join();
        EXPECT_TRUE(threads.empty());
        bridge.pump();
        return mailbox;
    }();
    // clang-format on

    ASSERT_EQ(threads.size(), 1U);
    EXPECT_EQ(threads.front(), std::this_thread::get_id());
    EXPECT_FALSE(kept.post([&threads] { threads.clear(); }));
    EXPECT_EQ(threads.size(), 1U);
}

TEST(BridgeTest, HandsOverRepliesEventsAndPostedWorkInTheOrderTheyArrived) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
    std::vector<std::string> order;
    const core::Connection progress = bridge.on("download.progress", [&order](const Bridge::Payload& payload) { order.push_back("progress " + payload.json.dump()); });
    const std::uint64_t call = bridge.call("download.start", {.json = core::Json::object()}, [&order](const Bridge::Result&) { order.emplace_back("done"); });
    EXPECT_TRUE(bridge.getMailbox().post([&order] { order.emplace_back("callback"); }));
    bridge.emit("download.progress", "1");
    bridge.resolve(call, true, "{}");
    bridge.emit("download.progress", "2");
    bridge.pump();
    EXPECT_EQ(order, (std::vector<std::string>{"callback", "progress 1", "done", "progress 2"}));
}

TEST(BridgeTest, LeavesNoCallBehindWhenItsHandlerThrows) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
    bridge.registerHandler("broken", [](const Bridge::Payload&, const Bridge::Reply&) { throw std::runtime_error("The handler failed."); });
    EXPECT_THROW((void)bridge.call("broken", {.json = core::Json::object()}, {}), std::runtime_error);
    EXPECT_EQ(bridge.getPendingCallCount(), 0U);
}

TEST(BridgeTest, KeepsLateRepliesAwayFromOtherBridges) {
    std::vector<std::string> answers;
    Bridge::Reply late;
    std::uint64_t oldCall = 0;
    {
        Bridge old([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
        old.registerHandler("slow", [&late](const Bridge::Payload&, Bridge::Reply reply) { late = std::move(reply); });
        oldCall = old.call("slow", {.json = core::Json::object()}, [&answers](Bridge::Result) { answers.emplace_back("old"); });
        EXPECT_EQ(old.getPendingCallCount(), 1U);
    }

    // The reply of a destroyed bridge goes nowhere, and a restarted app never shares call ids with the old one.
    late({.ok = true});
    Bridge restarted([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
    const std::uint64_t newCall = restarted.call("native.method", {.json = core::Json::object()}, [&answers](Bridge::Result result) { answers.push_back(result.value.json.get<std::string>()); });
    EXPECT_NE(newCall, oldCall);
    restarted.resolve(oldCall, true, R"("stale")");
    restarted.pump();
    EXPECT_TRUE(answers.empty());
    EXPECT_EQ(restarted.getPendingCallCount(), 1U);

    restarted.resolve(newCall, true, R"("fresh")");
    restarted.pump();
    EXPECT_EQ(answers, (std::vector<std::string>{"fresh"}));
    EXPECT_EQ(restarted.getPendingCallCount(), 0U);
}

TEST(BridgeTest, RoutesCallsToHandlersAndNativeCode) {
    test::EngineFixture fixture;
    Bridge& bridge = fixture.engine().getPlatform();

    std::vector<Bridge::Result> results;
    bridge.registerHandler("echo", [](const Bridge::Payload& params, Bridge::Reply reply) { reply({.ok = true, .value = params}); });
    bridge.call("echo", {.json = {{"value", 7}}}, [&](Bridge::Result result) { results.push_back(result); });
    EXPECT_TRUE(bridge.hasHandler("echo"));
    EXPECT_FALSE(bridge.hasHandler("shop.catalog"));

    const std::uint64_t native = bridge.call("shop.catalog", {.json = {{"detail", true}}}, [&](Bridge::Result result) { results.push_back(result); });
    const std::uint64_t failing = bridge.call("auth.login", {.json = core::Json::object()}, [&](Bridge::Result result) { results.push_back(result); });
    const std::uint64_t objectError = bridge.call("auth.logout", {.json = core::Json::object()}, [&](Bridge::Result result) { results.push_back(result); });
    const std::uint64_t invalid = bridge.call("broken", {.json = core::Json::object()}, [&](Bridge::Result result) { results.push_back(result); });
    bridge.call("fire.and.forget", {.json = core::Json::object()}, {});
    ASSERT_EQ(fixture.host().getPlatformCalls().size(), 5U);
    EXPECT_EQ(fixture.host().getPlatformCalls().front().method, "shop.catalog");
    EXPECT_EQ(fixture.host().getPlatformCalls().front().paramsJson, R"({"detail":true})");

    bridge.resolve(native, true, R"({"model": "test"})");
    bridge.resolve(failing, false, R"("cancelled")");
    bridge.resolve(objectError, false, R"({"message": "no session"})");
    bridge.resolve(invalid, true, "{broken");
    bridge.resolve(9999, true, "{}");
    EXPECT_EQ(bridge.getPendingCallCount(), 6U);
    fixture.frames(1);

    ASSERT_EQ(results.size(), 5U);
    EXPECT_EQ(results[0].value.json.at("value"), 7);
    EXPECT_EQ(results[1].value.json.at("model"), "test");
    EXPECT_EQ(results[2].error.message, "cancelled");
    EXPECT_EQ(results[3].error.message, "no session");
    EXPECT_FALSE(results[4].ok);
    EXPECT_EQ(bridge.getPendingCallCount(), 1U) << "A call without a callback waits for its answer too.";

    EXPECT_THROW(bridge.call("", {}, {}), std::invalid_argument);
    EXPECT_THROW(bridge.registerHandler("", {}), std::invalid_argument);
}

TEST(BridgeTest, RetainsEventsUntilTheFirstListenerConnects) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
    std::vector<core::Json> opened;
    std::vector<core::Json> purchases;

    // Retained events wait while nothing listens, the newest ones up to the limit, while plain events without a listener are dropped.
    for (int index = 0; index < 40; ++index) {
        bridge.emit("app.opened", std::to_string(index), {}, {.retain = true});
    }
    bridge.emit("app.opened", R"("plain")");
    bridge.emit("store.pending", R"({"id": 1})", {}, {.retain = true});
    bridge.pump();

    // The first listener receives them in order at the next pump, before a newer event of the same name.
    core::Connection first = bridge.on("app.opened", [&](const Bridge::Payload& payload) { opened.push_back(payload.json); });
    bridge.emit("app.opened", "40", {}, {.retain = true});
    EXPECT_TRUE(opened.empty());
    bridge.pump();
    ASSERT_EQ(opened.size(), Bridge::kRetainedLimit + 1);
    EXPECT_EQ(opened.front(), 40 - static_cast<int>(Bridge::kRetainedLimit));
    EXPECT_EQ(opened[Bridge::kRetainedLimit - 1], 39);
    EXPECT_EQ(opened.back(), 40);

    // Delivered events are gone, so a later listener hears only new ones.
    std::vector<core::Json> later;
    core::Connection second = bridge.on("app.opened", [&](const Bridge::Payload& payload) { later.push_back(payload.json); });
    bridge.pump();
    EXPECT_TRUE(later.empty());

    // A listener that leaves before the next pump leaves the events waiting for the next one.
    core::Connection gone = bridge.on("store.pending", [&](const Bridge::Payload& payload) { purchases.push_back(payload.json); });
    gone.disconnect();
    bridge.pump();
    EXPECT_TRUE(purchases.empty());
    core::Connection kept = bridge.on("store.pending", [&](const Bridge::Payload& payload) { purchases.push_back(payload.json); });
    bridge.pump();
    ASSERT_EQ(purchases.size(), 1U);
    EXPECT_EQ(purchases.front().at("id"), 1);
}

TEST(BridgeTest, SendsCallsWhoseAnswerNobodyNeeds) {
    std::vector<std::pair<std::uint64_t, std::string>> dispatched;
    Bridge bridge([&dispatched](std::uint64_t id, std::string_view method, std::string_view params, std::span<const std::vector<std::byte>>) { dispatched.emplace_back(id, std::string(method) + " " + std::string(params)); }, [](std::uint64_t, std::string_view) {});
    int counted = 0;
    // clang-format off
    bridge.registerHandler("engine.count", [&counted](const Bridge::Payload& params, Bridge::Reply reply) {
        counted += params.json.at("by").get<int>();
        reply({.ok = true});
    });
    // clang-format on

    bridge.send("native.track", {.json = {{"event", "start"}}});
    bridge.send("engine.count", {.json = {{"by", 2}}});
    EXPECT_EQ(bridge.getPendingCallCount(), 0U);
    ASSERT_EQ(dispatched.size(), 1U);
    EXPECT_EQ(dispatched.front().second, R"(native.track {"event":"start"})");
    EXPECT_EQ(counted, 2);

    // The native answer finds no call waiting for it and is dropped.
    bridge.resolve(dispatched.front().first, false, R"("failed")");
    bridge.pump();
    EXPECT_EQ(bridge.getPendingCallCount(), 0U);
    EXPECT_THROW(bridge.send("", {}), std::invalid_argument);
}

TEST(BridgeTest, DeliversNativeEventsToSubscribers) {
    test::EngineFixture fixture;
    Bridge& bridge = fixture.engine().getPlatform();

    std::vector<core::Json> payloads;
    core::Connection connection = bridge.on("app.link", [&](const Bridge::Payload& payload) { payloads.push_back(payload.json); });
    bridge.emit("app.link", R"({"url": "tinyisland://play"})");
    bridge.emit("app.link", "");
    bridge.emit("app.link", "{invalid");
    bridge.emit("nobody.listens", "{}");
    fixture.frames(1);

    ASSERT_EQ(payloads.size(), 2U);
    EXPECT_EQ(payloads[0].at("url"), "tinyisland://play");
    EXPECT_TRUE(payloads[1].is_null());

    connection.disconnect();
    bridge.emit("app.link", "{}");
    fixture.frames(1);
    EXPECT_EQ(payloads.size(), 2U);
    EXPECT_THROW(Bridge({}, {}), std::invalid_argument);
}

TEST(BridgeTest, CarriesByteBuffersBothWays) {
    std::string sentJson;
    std::vector<std::vector<std::byte>> sent;
    // clang-format off
    const auto dispatch = [&](std::uint64_t, std::string_view, std::string_view params, std::span<const std::vector<std::byte>> buffers) {
        sentJson = params;
        sent.assign(buffers.begin(), buffers.end());
    };
    // clang-format on
    Bridge bridge(dispatch, [](std::uint64_t, std::string_view) {});
    const std::vector<std::byte> photo{std::byte{0x89}, std::byte{'P'}, std::byte{0}, std::byte{0xFF}};
    std::vector<Bridge::Result> results;
    const auto record = [&results](Bridge::Result result) { results.push_back(std::move(result)); };

    const std::uint64_t call = bridge.call("camera.filter", {.json = {{"image", core::JsonBytes::makeReference(0)}, {"strength", 2}}, .buffers = {photo}}, record);
    EXPECT_EQ(sentJson, R"({"image":{"$bytes":0},"strength":2})");
    EXPECT_EQ(sent, std::vector<std::vector<std::byte>>{photo});

    // A reply refers to its buffers in any order and as often as it likes, and a failure keeps its JSON alone.
    bridge.resolve(call, true, R"({"thumbnail": {"$bytes": 1}, "original": {"$bytes": 0}, "again": {"$bytes": 1}})", {photo, {std::byte{7}}});
    const std::uint64_t broken = bridge.call("camera.filter", {}, record);
    bridge.resolve(broken, true, R"([{"$bytes": 2}])", {photo});
    const std::uint64_t failed = bridge.call("camera.filter", {}, record);
    bridge.resolve(failed, false, R"({"message": "no camera", "data": {"$bytes": 0}})", {photo});
    bridge.pump();

    ASSERT_EQ(results.size(), 3U);
    ASSERT_TRUE(results[0].ok);
    EXPECT_EQ(core::JsonBytes::findReference(results[0].value.json.at("thumbnail")), 1U);
    EXPECT_EQ(results[0].value.buffers, (std::vector<std::vector<std::byte>>{photo, {std::byte{7}}}));
    EXPECT_EQ(results[1].error.code, "invalidBytes");
    EXPECT_EQ(results[1].error.message, "The JSON refers to byte buffer 2, but only 1 came with it.");
    EXPECT_EQ(results[2].error.message, "no camera");
    EXPECT_TRUE(results[2].value.buffers.empty());

    // Parameters that refer to a buffer they lack never leave the engine, and neither does an event.
    EXPECT_THROW(bridge.call("camera.filter", {.json = core::JsonBytes::makeReference(0)}, {}), std::invalid_argument);
    EXPECT_THROW(bridge.send("camera.filter", {.json = {core::JsonBytes::makeReference(1)}, .buffers = {photo}}), std::invalid_argument);
    std::vector<Bridge::Payload> frames;
    core::Connection connection = bridge.on("camera.frame", [&](const Bridge::Payload& payload) { frames.push_back(payload); });
    bridge.emit("camera.frame", R"({"jpeg": {"$bytes": 0}})", {photo});
    bridge.emit("camera.frame", R"({"jpeg": {"$bytes": 1}})", {photo});
    bridge.pump();
    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0].buffers, std::vector<std::vector<std::byte>>{photo});
}

TEST(BridgeTest, GathersTheBatchedEventsOfAFrameIntoOneList) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view, std::span<const std::vector<std::byte>>) {}, [](std::uint64_t, std::string_view) {});
    std::vector<std::string> heard;
    std::vector<Bridge::Payload> readings;
    // clang-format off
    core::Connection sensor = bridge.on("sensor.reading", [&](const Bridge::Payload& payload) {
        heard.emplace_back("reading");
        readings.push_back(payload);
    });
    // clang-format on
    core::Connection state = bridge.on("sensor.state", [&](const Bridge::Payload& payload) { heard.push_back(payload.json.get<std::string>()); });

    // The batch arrives at the place of its first event, with the buffers of every event in order.
    bridge.emit("sensor.state", R"("waking")");
    bridge.emit("sensor.reading", R"({"x": 1, "raw": {"$bytes": 0}})", {{std::byte{1}}}, {.batched = true});
    bridge.emit("sensor.state", R"("calibrated")");
    bridge.emit("sensor.reading", R"({"x": 2, "raw": {"$bytes": 0}})", {{std::byte{2}}}, {.batched = true});
    bridge.emit("sensor.reading", R"({"x": 3})", {}, {.batched = true});
    bridge.pump();
    EXPECT_EQ(heard, (std::vector<std::string>{"waking", "reading", "calibrated"}));
    ASSERT_EQ(readings.size(), 1U);
    EXPECT_EQ(readings[0].json, core::Json::parse(R"([{"x": 1, "raw": {"$bytes": 0}}, {"x": 2, "raw": {"$bytes": 1}}, {"x": 3}])"));
    EXPECT_EQ(readings[0].buffers, (std::vector<std::vector<std::byte>>{{std::byte{1}}, {std::byte{2}}}));

    // A retained batch waits as the list of its frame, and the first listener receives each waiting list in order.
    for (int frame = 0; frame < 2; ++frame) {
        for (int index = 0; index < 3; ++index) {
            bridge.emit("gps.fix", std::to_string(frame * 3 + index), {}, {.retain = true, .batched = true});
        }
        bridge.pump();
    }
    bridge.emit("gps.fix", "99", {}, {.batched = true});
    bridge.pump();
    std::vector<core::Json> fixes;
    core::Connection gps = bridge.on("gps.fix", [&](const Bridge::Payload& payload) { fixes.push_back(payload.json); });
    bridge.pump();
    EXPECT_EQ(fixes, (std::vector<core::Json>{core::Json::parse("[0, 1, 2]"), core::Json::parse("[3, 4, 5]")}));
}

} // namespace haylen::platform
