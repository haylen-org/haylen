#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/platform/Bridge.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/GamepadSlots.hpp"
#include "platform/KeyboardTranslator.hpp"
#include "platform/sokol/MemoryWarning.hpp"
#include "platform/sokol/SokolEvents.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class SokolEventsTest : public ::testing::Test {
  protected:
    [[nodiscard]] static sapp_event makeEvent(sapp_event_type type) {
        sapp_event event{};
        event.type = type;
        return event;
    }
};

TEST_F(SokolEventsTest, TranslatesKeysTextAndMouse) {
    sapp_event key = makeEvent(SAPP_EVENTTYPE_KEY_DOWN);
    key.key_code = SAPP_KEYCODE_SPACE;
    key.key_repeat = true;
    key.modifiers = SAPP_MODIFIER_SHIFT | SAPP_MODIFIER_CTRL | SAPP_MODIFIER_ALT | SAPP_MODIFIER_SUPER;
    const Event down = *SokolEvents::translate(key);
    EXPECT_EQ(down.type, Event::Type::KeyDown);
    EXPECT_EQ(down.key, input::Key::Space);
    EXPECT_TRUE(down.repeat);
    EXPECT_EQ(down.modifiers, (input::KeyModifiers{.shift = true, .control = true, .alt = true, .super = true}));

    key.type = SAPP_EVENTTYPE_KEY_UP;
    key.key_code = SAPP_KEYCODE_ESCAPE;
    key.modifiers = 0;
    EXPECT_EQ(SokolEvents::translate(key)->key, input::Key::Escape);
    EXPECT_EQ(SokolEvents::translate(key)->modifiers, input::KeyModifiers{});

    sapp_event character = makeEvent(SAPP_EVENTTYPE_CHAR);
    character.char_code = 0x00E9;
    EXPECT_EQ(SokolEvents::translate(character)->character, U'é');

    sapp_event mouse = makeEvent(SAPP_EVENTTYPE_MOUSE_DOWN);
    mouse.mouse_button = SAPP_MOUSEBUTTON_RIGHT;
    mouse.mouse_x = 10.0F;
    mouse.mouse_y = 20.0F;
    const Event pressed = *SokolEvents::translate(mouse);
    EXPECT_EQ(pressed.mouseButton, input::MouseButton::Right);
    EXPECT_EQ(pressed.position, math::Vec2(10.0F, 20.0F));
    mouse.type = SAPP_EVENTTYPE_MOUSE_UP;
    mouse.mouse_button = SAPP_MOUSEBUTTON_MIDDLE;
    EXPECT_EQ(SokolEvents::translate(mouse)->mouseButton, input::MouseButton::Middle);
    mouse.mouse_button = SAPP_MOUSEBUTTON_LEFT;
    EXPECT_EQ(SokolEvents::translate(mouse)->type, Event::Type::MouseUp);

    sapp_event move = makeEvent(SAPP_EVENTTYPE_MOUSE_MOVE);
    move.mouse_dx = 3.0F;
    move.mouse_dy = -2.0F;
    EXPECT_EQ(SokolEvents::translate(move)->delta, math::Vec2(3.0F, -2.0F));

    sapp_event scroll = makeEvent(SAPP_EVENTTYPE_MOUSE_SCROLL);
    scroll.scroll_y = 1.5F;
    EXPECT_EQ(SokolEvents::translate(scroll)->scroll, math::Vec2(0.0F, 1.5F));
}

TEST_F(SokolEventsTest, TranslatesTouchesAndLifecycle) {
    sapp_event touches = makeEvent(SAPP_EVENTTYPE_TOUCHES_BEGAN);
    touches.num_touches = 2;
    touches.touches[0] = {.identifier = 11, .pos_x = 1.0F, .pos_y = 2.0F, .changed = true};
    touches.touches[1] = {.identifier = 12, .pos_x = 3.0F, .pos_y = 4.0F, .changed = false};
    const Event began = *SokolEvents::translate(touches);
    ASSERT_EQ(began.touchCount, 2U);
    EXPECT_EQ(began.touches[1].id, 12U);
    EXPECT_EQ(began.touches[0].position, math::Vec2(1.0F, 2.0F));
    EXPECT_TRUE(began.touches[0].changed);

    const std::vector<std::pair<sapp_event_type, Event::Type>> mapping{
        {SAPP_EVENTTYPE_TOUCHES_MOVED, Event::Type::TouchMoved}, {SAPP_EVENTTYPE_TOUCHES_ENDED, Event::Type::TouchEnded}, {SAPP_EVENTTYPE_TOUCHES_CANCELLED, Event::Type::TouchCancelled}, {SAPP_EVENTTYPE_MOUSE_ENTER, Event::Type::MouseEnter}, {SAPP_EVENTTYPE_MOUSE_LEAVE, Event::Type::MouseLeave}, {SAPP_EVENTTYPE_RESIZED, Event::Type::Resized}, {SAPP_EVENTTYPE_ICONIFIED, Event::Type::Suspended}, {SAPP_EVENTTYPE_SUSPENDED, Event::Type::Suspended}, {SAPP_EVENTTYPE_RESTORED, Event::Type::Resumed}, {SAPP_EVENTTYPE_RESUMED, Event::Type::Resumed}, {SAPP_EVENTTYPE_FOCUSED, Event::Type::FocusGained}, {SAPP_EVENTTYPE_UNFOCUSED, Event::Type::FocusLost}, {SAPP_EVENTTYPE_QUIT_REQUESTED, Event::Type::QuitRequested},
    };
    for (const auto& [source, expected] : mapping) {
        EXPECT_EQ(SokolEvents::translate(makeEvent(source))->type, expected) << static_cast<int>(source);
    }
    EXPECT_FALSE(SokolEvents::translate(makeEvent(SAPP_EVENTTYPE_CLIPBOARD_PASTED)).has_value());
    EXPECT_FALSE(SokolEvents::translate(makeEvent(SAPP_EVENTTYPE_FILES_DROPPED)).has_value());
}

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
    EXPECT_EQ(describe(edit("hi th", 3, 5)), " ") << "text inside an open composition waits";
    EXPECT_EQ(describe(edit("hi there")), "there");
    EXPECT_EQ(describe(edit("hi their")), backspace + backspace + "ir") << "a corrected word is erased and typed again";
    EXPECT_EQ(describe(edit("hi their\n")), enter);
    EXPECT_EQ(describe(Event{.type = Event::Type::TextAction, .textAction = TextInput::Action::Submit}), enter);
    EXPECT_EQ(describe(Event{.type = Event::Type::TextAction, .textAction = TextInput::Action::Dismissed}), "");

    keyboard.reset();
    EXPECT_EQ(describe(edit("new")), "new");
}

TEST(BridgeRelayTest, ForwardsNativeRepliesToTheAttachedBridge) {
    std::vector<std::string> calls;
    Bridge bridge([&calls](std::uint64_t, std::string_view method, std::string_view) { calls.emplace_back(method); }, [](std::uint64_t, std::string_view) {});
    std::vector<Bridge::Result> results;
    std::vector<core::Json> events;
    const std::uint64_t call = bridge.call("native.method", core::Json::object(), [&](Bridge::Result result) { results.push_back(std::move(result)); });
    core::Connection connection = bridge.on("native.event", [&](const core::Json& payload) { events.push_back(payload); });

    BridgeRelay::resolve(call, true, "1");
    BridgeRelay::emit("native.event", "2", false);
    BridgeRelay::emit("native.late", "5", true);
    bridge.pump();
    EXPECT_TRUE(results.empty()) << "nothing reaches a bridge that is not attached";

    BridgeRelay::attach(bridge);
    BridgeRelay::resolve(call, true, "{\"value\": 3}");
    BridgeRelay::emit("native.event", "4", false);
    BridgeRelay::emit("native.late", "6", true);
    BridgeRelay::detach(bridge);
    bridge.pump();
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results.front().value.at("value"), 3);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events.front(), 4);
    EXPECT_EQ(calls, (std::vector<std::string>{"native.method"}));

    // The relay keeps the retain flag, so the event that came while the bridge was attached waits for its first listener.
    core::Connection late = bridge.on("native.late", [&](const core::Json& payload) { events.push_back(payload); });
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

TEST(GamepadSlotsTest, KeepsEveryControllerInItsSlotWhileItStaysConnected) {
    std::array<int, 6> pads{};
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
    slots.update(std::vector{pad(1), pad(3), pad(2), pad(4), pad(5)});
    EXPECT_EQ(slots.getController(0), pad(3));
    EXPECT_EQ(slots.getController(3), pad(4));
    slots.update(std::vector{pad(3), pad(2), pad(4), pad(5)});
    EXPECT_EQ(slots.getController(1), pad(5));
    EXPECT_EQ(slots.getController(input::Input::kMaxGamepads), nullptr);

    slots.update({});
    for (std::size_t slot = 0; slot < input::Input::kMaxGamepads; ++slot) {
        EXPECT_EQ(slots.getController(slot), nullptr);
    }
}

TEST(BridgeTest, KeepsTheMessageCodeAndDataOfEveryFailure) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view) {}, [](std::uint64_t, std::string_view) {});
    std::vector<Bridge::Result> results;
    std::vector<std::uint64_t> calls;
    for (int index = 0; index < 9; ++index) {
        calls.push_back(bridge.call("native.method", core::Json::object(), [&results](Bridge::Result result) { results.push_back(std::move(result)); }));
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
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view) {}, [&cancelled](std::uint64_t id, std::string_view method) { cancelled.emplace_back(id, method); });
    Bridge::Reply late;
    bridge.registerHandler("engine.slow", [&late](const core::Json&, Bridge::Reply reply) { late = std::move(reply); });

    std::vector<std::string> codes;
    const auto record = [&codes](Bridge::Result result) { codes.push_back(result.ok ? "ok" : result.error.code.get<std::string>()); };
    const std::uint64_t quick = bridge.call("native.quick", core::Json::object(), record, std::chrono::milliseconds(1));
    const std::uint64_t answered = bridge.call("native.answered", core::Json::object(), record, std::chrono::hours(1));
    const std::uint64_t dropped = bridge.call("native.dropped", core::Json::object(), record);
    bridge.call("engine.slow", core::Json::object(), record, std::chrono::milliseconds(1));
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

TEST(BridgeTest, RunsPostedWorkOnTheFrameThreadUntilTheBridgeIsGone) {
    std::vector<std::thread::id> threads;
    // clang-format off
    Bridge::Mailbox kept = [&threads] {
        Bridge bridge([](std::uint64_t, std::string_view, std::string_view) {}, [](std::uint64_t, std::string_view) {});
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

TEST(BridgeTest, KeepsLateRepliesAwayFromOtherBridges) {
    std::vector<std::string> answers;
    Bridge::Reply late;
    std::uint64_t oldCall = 0;
    {
        Bridge old([](std::uint64_t, std::string_view, std::string_view) {}, [](std::uint64_t, std::string_view) {});
        old.registerHandler("slow", [&late](const core::Json&, Bridge::Reply reply) { late = std::move(reply); });
        oldCall = old.call("slow", core::Json::object(), [&answers](Bridge::Result) { answers.emplace_back("old"); });
        EXPECT_EQ(old.getPendingCallCount(), 1U);
    }

    // The reply of a destroyed bridge goes nowhere, and a restarted app never shares call ids with the old one.
    late({.ok = true});
    Bridge restarted([](std::uint64_t, std::string_view, std::string_view) {}, [](std::uint64_t, std::string_view) {});
    const std::uint64_t newCall = restarted.call("native.method", core::Json::object(), [&answers](Bridge::Result result) { answers.push_back(result.value.get<std::string>()); });
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
    bridge.registerHandler("echo", [](const core::Json& params, Bridge::Reply reply) { reply({.ok = true, .value = params}); });
    bridge.call("echo", {{"value", 7}}, [&](Bridge::Result result) { results.push_back(result); });
    EXPECT_TRUE(bridge.hasHandler("echo"));
    EXPECT_FALSE(bridge.hasHandler("shop.catalog"));

    const std::uint64_t native = bridge.call("shop.catalog", {{"detail", true}}, [&](Bridge::Result result) { results.push_back(result); });
    const std::uint64_t failing = bridge.call("auth.login", core::Json::object(), [&](Bridge::Result result) { results.push_back(result); });
    const std::uint64_t objectError = bridge.call("auth.logout", core::Json::object(), [&](Bridge::Result result) { results.push_back(result); });
    const std::uint64_t invalid = bridge.call("broken", core::Json::object(), [&](Bridge::Result result) { results.push_back(result); });
    bridge.call("fire.and.forget", core::Json::object(), {});
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
    EXPECT_EQ(results[0].value.at("value"), 7);
    EXPECT_EQ(results[1].value.at("model"), "test");
    EXPECT_EQ(results[2].error.message, "cancelled");
    EXPECT_EQ(results[3].error.message, "no session");
    EXPECT_FALSE(results[4].ok);
    EXPECT_EQ(bridge.getPendingCallCount(), 1U) << "a call without a callback waits for its answer too";

    EXPECT_THROW(bridge.call("", core::Json::object(), {}), std::invalid_argument);
    EXPECT_THROW(bridge.registerHandler("", {}), std::invalid_argument);
}

TEST(BridgeTest, RetainsEventsUntilTheFirstListenerConnects) {
    Bridge bridge([](std::uint64_t, std::string_view, std::string_view) {}, [](std::uint64_t, std::string_view) {});
    std::vector<core::Json> opened;
    std::vector<core::Json> purchases;

    // Retained events wait while nothing listens, the newest ones up to the limit, while plain events without a listener are dropped.
    for (int index = 0; index < 40; ++index) {
        bridge.emit("app.opened", std::to_string(index), true);
    }
    bridge.emit("app.opened", R"("plain")");
    bridge.emit("store.pending", R"({"id": 1})", true);
    bridge.pump();

    // The first listener receives them in order at the next pump, before a newer event of the same name.
    core::Connection first = bridge.on("app.opened", [&](const core::Json& payload) { opened.push_back(payload); });
    bridge.emit("app.opened", "40", true);
    EXPECT_TRUE(opened.empty());
    bridge.pump();
    ASSERT_EQ(opened.size(), Bridge::kRetainedLimit + 1);
    EXPECT_EQ(opened.front(), 40 - static_cast<int>(Bridge::kRetainedLimit));
    EXPECT_EQ(opened[Bridge::kRetainedLimit - 1], 39);
    EXPECT_EQ(opened.back(), 40);

    // Delivered events are gone, so a later listener hears only new ones.
    std::vector<core::Json> later;
    core::Connection second = bridge.on("app.opened", [&](const core::Json& payload) { later.push_back(payload); });
    bridge.pump();
    EXPECT_TRUE(later.empty());

    // A listener that leaves before the next pump leaves the events waiting for the next one.
    core::Connection gone = bridge.on("store.pending", [&](const core::Json& payload) { purchases.push_back(payload); });
    gone.disconnect();
    bridge.pump();
    EXPECT_TRUE(purchases.empty());
    core::Connection kept = bridge.on("store.pending", [&](const core::Json& payload) { purchases.push_back(payload); });
    bridge.pump();
    ASSERT_EQ(purchases.size(), 1U);
    EXPECT_EQ(purchases.front().at("id"), 1);
}

TEST(BridgeTest, SendsCallsWhoseAnswerNobodyNeeds) {
    std::vector<std::pair<std::uint64_t, std::string>> dispatched;
    Bridge bridge([&dispatched](std::uint64_t id, std::string_view method, std::string_view params) { dispatched.emplace_back(id, std::string(method) + " " + std::string(params)); }, [](std::uint64_t, std::string_view) {});
    int counted = 0;
    // clang-format off
    bridge.registerHandler("engine.count", [&counted](const core::Json& params, Bridge::Reply reply) {
        counted += params.at("by").get<int>();
        reply({.ok = true});
    });
    // clang-format on

    bridge.send("native.track", {{"event", "start"}});
    bridge.send("engine.count", {{"by", 2}});
    EXPECT_EQ(bridge.getPendingCallCount(), 0U);
    ASSERT_EQ(dispatched.size(), 1U);
    EXPECT_EQ(dispatched.front().second, R"(native.track {"event":"start"})");
    EXPECT_EQ(counted, 2);

    // The native answer finds no call waiting for it and is dropped.
    bridge.resolve(dispatched.front().first, false, R"("failed")");
    bridge.pump();
    EXPECT_EQ(bridge.getPendingCallCount(), 0U);
    EXPECT_THROW(bridge.send("", core::Json::object()), std::invalid_argument);
}

TEST(BridgeTest, DeliversNativeEventsToSubscribers) {
    test::EngineFixture fixture;
    Bridge& bridge = fixture.engine().getPlatform();

    std::vector<core::Json> payloads;
    core::Connection connection = bridge.on("app.link", [&](const core::Json& payload) { payloads.push_back(payload); });
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

} // namespace haylen::platform
