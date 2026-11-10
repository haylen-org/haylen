#include "platform/web/WebOutputs.hpp"

#include <emscripten/em_macros.h>
#include <emscripten/emscripten.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "platform/Player.hpp"
#include "platform/web/WebEntry.hpp"

// clang-format off
EM_JS_DEPS(haylen_web_outputs, "$specialHTMLTargets");

EM_JS(void, haylen_js_open_output, (int number, const char* display, const char* title, int fullscreen, int width, int height), {
    Module.haylen.openOutput(number, UTF8ToString(display), UTF8ToString(title), fullscreen !== 0, width, height);
});

EM_JS(void, haylen_js_close_output, (int number), {
    delete specialHTMLTargets["!haylen-output-" + number];
    Module.haylen.closeOutput(number);
});

EM_JS(void, haylen_js_name_output_canvas, (int number), {
    specialHTMLTargets["!haylen-output-" + number] = Module.haylen.outputCanvas(number);
});
// clang-format on

namespace haylen::platform {

std::unordered_map<std::uint32_t, WebOutputs::Entry>& WebOutputs::entries = *new std::unordered_map<std::uint32_t, Entry>();

void WebOutputs::open(std::uint32_t number, std::string_view display, const Output::Options& options, int sampleCount) {
    entries[number] = {.canvas = "!haylen-output-" + std::to_string(number), .sampleCount = sampleCount};
    haylen_js_open_output(static_cast<int>(number), std::string(display).c_str(), options.title.c_str(), options.fullscreen ? 1 : 0, static_cast<int>(std::lround(options.size.x)), static_cast<int>(std::lround(options.size.y)));
}

void WebOutputs::close(std::uint32_t number) {
    if (entries.erase(number) > 0) {
        haylen_js_close_output(static_cast<int>(number));
    }
}

Host::OutputReport WebOutputs::getReport(std::uint32_t number) {
    const auto found = entries.find(number);
    return found != entries.end() ? found->second.report : Host::OutputReport{.ending = Output::Ending::App};
}

// The WebGPU device finds the canvas of the window of the output under a name of its own, since it lives in the document of that window.
void WebOutputs::receiveCanvas(std::uint32_t number, int width, int height, float scale) {
    const auto found = entries.find(number);
    if (found == entries.end() || found->second.report.ending) {
        return;
    }
    Entry& entry = found->second;
    if (!entry.report.surface) {
        haylen_js_name_output_canvas(static_cast<int>(number));
    }
    entry.report.surface = gpu::Surface{.kind = gpu::Surface::Kind::WebCanvas, .canvas = entry.canvas, .size = {.width = width, .height = height, .windowWidth = width, .windowHeight = height}, .sampleCount = entry.sampleCount};
    entry.report.scale = scale;
    Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
}

void WebOutputs::receiveEnding(std::uint32_t number, Output::Ending ending, std::string reason) {
    const auto found = entries.find(number);
    if (found == entries.end() || found->second.report.ending) {
        return;
    }
    found->second.report = {.ending = ending, .reason = std::move(reason)};
    Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
}

void WebOutputs::receivePointer(std::uint32_t number, Event event) {
    if (entries.contains(number)) {
        event.output = number;
        Player::postEvent(event);
    }
}

} // namespace haylen::platform

extern "C" {
EMSCRIPTEN_KEEPALIVE void haylen_web_output_canvas(int number, int width, int height, float scale) {
    haylen::platform::WebEntry::guard("haylen_web_output_canvas", [&] { haylen::platform::WebOutputs::receiveCanvas(static_cast<std::uint32_t>(number), width, height, scale); });
}

EMSCRIPTEN_KEEPALIVE void haylen_web_output_ended(int number, int ending, const char* reason) {
    haylen::platform::WebEntry::guard("haylen_web_output_ended", [&] { haylen::platform::WebOutputs::receiveEnding(static_cast<std::uint32_t>(number), static_cast<haylen::platform::Output::Ending>(ending), reason); });
}

// The kinds of pointer events are a press, a release, a move and a scroll, with the button as the browser numbers it.
EMSCRIPTEN_KEEPALIVE void haylen_web_output_pointer(int number, int kind, float x, float y, int button, float scrollX, float scrollY) {
    // clang-format off
    haylen::platform::WebEntry::guard("haylen_web_output_pointer", [&] {
        using haylen::platform::Event;
        constexpr std::array<Event::Type, 4> kTypes{Event::Type::MouseDown, Event::Type::MouseUp, Event::Type::MouseMove, Event::Type::MouseScroll};
        const haylen::input::MouseButton pressed = button == 2 ? haylen::input::MouseButton::Right : (button == 1 ? haylen::input::MouseButton::Middle : haylen::input::MouseButton::Left);
        haylen::platform::WebOutputs::receivePointer(static_cast<std::uint32_t>(number), {.type = kTypes.at(static_cast<std::size_t>(kind)), .mouseButton = pressed, .position = {x, y}, .scroll = {scrollX, scrollY}});
    });
    // clang-format on
}
}
