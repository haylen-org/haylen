#include "haylen/platform/Outputs.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/core/EventBus.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/platform/Displays.hpp"
#include "platform/Host.hpp"

namespace haylen::platform {

Outputs::Outputs(Host& source, Displays& known, graphics::Device& device, core::EventBus& bus) : host(source), displays(known), graphics(device), events(bus) {}

// The app that stops lets go of its outputs without hearing of them.
Outputs::~Outputs() {
    for (const std::shared_ptr<Output>& output : outputs) {
        releaseSurface(*output);
        host.closeOutput(output->id);
        output->state = Output::State::Closed;
    }
}

std::shared_ptr<Output> Outputs::open(std::string_view display, const Output::Options& options) {
    if (displays.find(display) == nullptr) {
        throw std::invalid_argument("There is no display \"" + std::string(display) + "\" to open an output on. The function \"display.list()\" lists the displays.");
    }
    if (display == displays.getWindowDisplay().id) {
        throw std::invalid_argument("The display \"" + std::string(display) + "\" shows the window of the app, so it shows no output.");
    }
    if (std::ranges::any_of(outputs, [display](const std::shared_ptr<Output>& open) { return open->displayId == display; })) {
        throw std::invalid_argument("The display \"" + std::string(display) + "\" shows another output already.");
    }

    auto output = std::make_shared<Output>(nextId++, std::string(display), options.clearColor);
    outputs.push_back(output);
    host.openOutput(output->id, display, options);
    return output;
}

void Outputs::close(Output& output) {
    if (output.state == Output::State::Closed) {
        return;
    }
    end(output, Output::Ending::App, {});
}

std::span<const std::shared_ptr<Output>> Outputs::getAll() const noexcept {
    return outputs;
}

Output* Outputs::find(std::uint32_t number) const noexcept {
    const auto found = std::ranges::find(outputs, number, [](const std::shared_ptr<Output>& output) { return output->id; });
    return found != outputs.end() ? found->get() : nullptr;
}

// A report that comes after the app closed the output finds nothing to change.
void Outputs::refresh(std::uint32_t number) {
    Output* output = find(number);
    if (output == nullptr) {
        return;
    }
    const Host::OutputReport report = host.getOutputReport(number);
    if (report.ending) {
        end(*output, *report.ending, report.reason);
        return;
    }
    if (!report.surface) {
        hide(*output);
        return;
    }
    show(*output, *report.surface, report.scale);
}

// A display leaves the list when the platform no longer shows the desktop on it, whether it was unplugged or turned off, so its output ends however the platform reported the change. The loop walks a snapshot, since each ending shortens the list and runs the listeners of the app.
void Outputs::followDisplays() {
    snapshot.assign(outputs.begin(), outputs.end());
    for (const std::shared_ptr<Output>& output : snapshot) {
        if (output->state != Output::State::Closed && displays.find(output->displayId) == nullptr) {
            end(*output, Output::Ending::Disconnected, {});
        }
    }
    snapshot.clear();
}

// A GPU device that draws into no surface beyond the window ends the output, since nothing can show it.
void Outputs::show(Output& output, const gpu::Surface& surface, float scale) {
    gpu::Device& gpu = graphics.getGpu();
    const math::Vec2 size{static_cast<float>(surface.width), static_cast<float>(surface.height)};
    if (output.surface == 0) {
        try {
            output.surface = gpu.addSurface(surface);
        } catch (const std::runtime_error& error) {
            end(output, Output::Ending::Unsupported, error.what());
            return;
        }
    } else if (size != output.surfaceSize) {
        gpu.resizeSurface(output.surface, surface.width, surface.height);
    }

    const bool resized = size != output.surfaceSize;
    output.surfaceSize = size;
    output.scale = scale;
    if (resized && surface.width > 0 && surface.height > 0) {
        output.target = graphics.createRenderTarget(surface.width, surface.height);
    }

    const Output::State previous = std::exchange(output.state, Output::State::Shown);
    const core::Json described{{"output", output.id}, {"display", output.displayId}, {"width", surface.width}, {"height", surface.height}, {"scale", core::JsonNumber::fromFloat(scale)}};
    if (previous != Output::State::Shown) {
        events.emit(core::LifecycleEvent::kOutputShown, described);
    } else if (resized) {
        events.emit(core::LifecycleEvent::kOutputResized, described);
    }
}

void Outputs::hide(Output& output) {
    releaseSurface(output);
    if (output.state != Output::State::Hidden) {
        output.state = Output::State::Hidden;
        events.emit(core::LifecycleEvent::kOutputHidden, {{"output", output.id}});
    }
}

void Outputs::releaseSurface(Output& output) {
    if (output.surface != 0) {
        graphics.getGpu().removeSurface(output.surface);
        output.surface = 0;
        output.surfaceSize = {};
    }
}

// The GPU device lets go of the surface before the platform forgets the output and closes what showed it, however it ended.
void Outputs::end(Output& output, Output::Ending ending, std::string_view reason) {
    releaseSurface(output);
    host.closeOutput(output.id);
    output.state = Output::State::Closed;
    output.ending = ending;
    const std::string_view name = std::ranges::find(Output::kEndingNames, ending, &std::pair<std::string_view, Output::Ending>::second)->first;
    if (!reason.empty()) {
        core::Log::warning("The output {} on the display \"{}\" closed: {}", output.id, output.displayId, reason);
    }

    // The event goes out once the output left the list, so a listener that opens another output on the display finds it free.
    const std::shared_ptr<Output> kept = *std::ranges::find_if(outputs, [&output](const std::shared_ptr<Output>& open) { return open.get() == &output; });
    std::erase(outputs, kept);
    core::Json described{{"output", kept->id}, {"display", kept->displayId}, {"ending", name}};
    if (!reason.empty()) {
        described["reason"] = std::string(reason);
    }
    events.emit(core::LifecycleEvent::kOutputClosed, described);
}

// The pointer over an output reaches the app in the points of the output, apart from the input of the window.
void Outputs::receiveInput(const Event& event) {
    if (find(event.output) == nullptr) {
        return;
    }
    const std::string_view type = std::ranges::find(Event::kTypeNames, event.type, &std::pair<std::string_view, Event::Type>::second)->first;
    core::Json described{{"output", event.output}, {"type", type}, {"x", core::JsonNumber::fromFloat(event.position.x)}, {"y", core::JsonNumber::fromFloat(event.position.y)}};
    switch (event.type) {
    case Event::Type::MouseDown:
    case Event::Type::MouseUp:
        described["button"] = input::Controls::mouseButtonName(event.mouseButton);
        break;
    case Event::Type::MouseScroll:
        described["scrollX"] = core::JsonNumber::fromFloat(event.scroll.x);
        described["scrollY"] = core::JsonNumber::fromFloat(event.scroll.y);
        break;
    case Event::Type::TouchBegan:
    case Event::Type::TouchMoved:
    case Event::Type::TouchEnded:
    case Event::Type::TouchCancelled: {
        core::Json touches = core::Json::array();
        for (std::size_t index = 0; index < event.touchCount; ++index) {
            const TouchPoint& touch = event.touches[index];
            touches.push_back({{"id", touch.id}, {"x", core::JsonNumber::fromFloat(touch.position.x)}, {"y", core::JsonNumber::fromFloat(touch.position.y)}, {"changed", touch.changed}});
        }
        described["touches"] = std::move(touches);
        break;
    }
    default:
        break;
    }
    events.emit(core::LifecycleEvent::kOutputInput, described);
}

} // namespace haylen::platform
