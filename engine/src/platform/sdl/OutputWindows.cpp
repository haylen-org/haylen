#include "platform/sdl/OutputWindows.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_properties.h>

#include "platform/Player.hpp"
#include "platform/sdl/Host.hpp"
#include "platform/sdl/Window.hpp"

namespace haylen::platform::sdl {

OutputWindows::~OutputWindows() {
    closeAll();
}

// A window that SDL or the graphics API cannot open on the display ends the output with the reason. A window of the size of `options` in points comes as large on the desktops whose window coordinates are pixels.
void OutputWindows::open(std::uint32_t number, const Display& display, const Output::Options& options, const Window& main) {
    const SDL_DisplayID target = Window::findDisplay(display);
    Entry& entry = entries.emplace_back(Entry{.number = number});
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!options.fullscreen) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    switch (main.getApi()) {
    case gpu::Device::Api::Metal:
        flags |= SDL_WINDOW_METAL;
        break;
    case gpu::Device::Api::Vulkan:
        flags |= SDL_WINDOW_VULKAN;
        break;
    case gpu::Device::Api::OpenGl:
    case gpu::Device::Api::OpenGlEs:
        flags |= SDL_WINDOW_OPENGL;
        break;
    default:
        break;
    }

    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, options.title.c_str());
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, static_cast<std::int64_t>(SDL_WINDOWPOS_CENTERED_DISPLAY(target)));
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, static_cast<std::int64_t>(SDL_WINDOWPOS_CENTERED_DISPLAY(target)));
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, std::lround(options.size.x));
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, std::lround(options.size.y));
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, static_cast<std::int64_t>(flags));
    entry.window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (entry.window == nullptr) {
        entry.report = {.ending = Output::Ending::Unsupported, .reason = "The window of the output could not open. " + std::string(SDL_GetError())};
        Player::postEvent({.type = platform::Event::Type::OutputChanged, .output = number});
        return;
    }

    const float units = SDL_GetWindowDisplayScale(entry.window) / SDL_GetWindowPixelDensity(entry.window);
    if (units != 1.0F) {
        SDL_SetWindowSize(entry.window, static_cast<int>(std::lround(options.size.x * units)), static_cast<int>(std::lround(options.size.y * units)));
    }
    if (main.getApi() == gpu::Device::Api::Metal) {
        entry.view = SDL_Metal_CreateView(entry.window);
    }
    if (options.fullscreen) {
        SDL_SetWindowFullscreen(entry.window, true);
    }
    SDL_ShowWindow(entry.window);

    if (main.getGlContext() != nullptr) {
        entry.gl = std::make_unique<GlContext>(entry.window, *main.getGlContext());
        entry.report.surface = gpu::Surface{.kind = gpu::Surface::Kind::Gl, .gl = entry.gl.get()};
    } else {
        entry.report.surface = Window::describe(entry.window, entry.view);
    }
    entry.report.surface->sampleCount = main.getSampleCount();
    entry.report.surface->vsync = main.isVsync();
    refresh(entry);
    Player::postEvent({.type = platform::Event::Type::OutputChanged, .output = number});
}

void OutputWindows::close(std::uint32_t number) noexcept {
    if (Entry* entry = findNumber(number)) {
        destroy(*entry);
        std::erase_if(entries, [number](const Entry& kept) { return kept.number == number; });
    }
}

void OutputWindows::closeAll() noexcept {
    for (Entry& entry : entries) {
        destroy(entry);
    }
    entries.clear();
}

Host::OutputReport OutputWindows::getReport(std::uint32_t number) const {
    const auto found = std::ranges::find(entries, number, &Entry::number);
    return found != entries.end() ? found->report : Host::OutputReport{.ending = Output::Ending::App};
}

bool OutputWindows::holdsFocus() const noexcept {
    SDL_Window* focused = SDL_GetKeyboardFocus();
    return focused != nullptr && std::ranges::any_of(entries, [focused](const Entry& entry) { return entry.window == focused; });
}

OutputWindows::Entry* OutputWindows::findWindow(SDL_WindowID id) noexcept {
    const auto found = std::ranges::find_if(entries, [id](const Entry& entry) { return entry.window != nullptr && SDL_GetWindowID(entry.window) == id; });
    return found != entries.end() ? &*found : nullptr;
}

OutputWindows::Entry* OutputWindows::findNumber(std::uint32_t number) noexcept {
    const auto found = std::ranges::find(entries, number, &Entry::number);
    return found != entries.end() ? &*found : nullptr;
}

void OutputWindows::destroy(Entry& entry) noexcept {
    entry.gl.reset();
    if (entry.view != nullptr) {
        SDL_Metal_DestroyView(entry.view);
        entry.view = nullptr;
    }
    if (entry.window != nullptr) {
        SDL_DestroyWindow(entry.window);
        entry.window = nullptr;
    }
}

// The surface takes the size of the drawable, and the scale of the display that the window is on.
void OutputWindows::refresh(Entry& entry) {
    SDL_GetWindowSizeInPixels(entry.window, &entry.report.surface->width, &entry.report.surface->height);
    entry.report.scale = SDL_GetWindowDisplayScale(entry.window);
}

// The window hides at once, and the report of the ending keeps no surface, so the output lets go of it before the window goes with the output.
void OutputWindows::end(Entry& entry, Output::Ending ending) {
    SDL_HideWindow(entry.window);
    entry.report = {.ending = ending};
    Player::postEvent({.type = platform::Event::Type::OutputChanged, .output = entry.number});
}

bool OutputWindows::handle(const SDL_Event& event) {
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
        Entry* entry = findWindow(event.window.windowID);
        if (entry != nullptr && !entry->report.ending) {
            handleWindow(*entry, event);
        }
        return entry != nullptr;
    }

    SDL_WindowID window = 0;
    switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        window = event.button.windowID;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        window = event.motion.windowID;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        window = event.wheel.windowID;
        break;
    default:
        return false;
    }
    const Entry* entry = findWindow(window);
    if (entry != nullptr) {
        handlePointer(*entry, event);
    }
    return entry != nullptr;
}

// A window that the person closes ends its output, and the window of the app stays.
void OutputWindows::handleWindow(Entry& entry, const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
        refresh(entry);
        Player::postEvent({.type = platform::Event::Type::OutputChanged, .output = entry.number});
        return;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        end(entry, Output::Ending::Person);
        return;
    default:
        return;
    }
}

// SDL places the pointer in the units of the window, which are its points on macOS and its pixels on Windows and X11.
void OutputWindows::handlePointer(const Entry& entry, const SDL_Event& event) {
    const float points = SDL_GetWindowPixelDensity(entry.window) / SDL_GetWindowDisplayScale(entry.window);
    switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const platform::Event::Type type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? platform::Event::Type::MouseDown : platform::Event::Type::MouseUp;
        const input::MouseButton button = event.button.button == SDL_BUTTON_RIGHT ? input::MouseButton::Right : (event.button.button == SDL_BUTTON_MIDDLE ? input::MouseButton::Middle : input::MouseButton::Left);
        deliver(entry, {.type = type, .mouseButton = button}, event.button.x * points, event.button.y * points);
        return;
    }
    case SDL_EVENT_MOUSE_MOTION:
        deliver(entry, {.type = platform::Event::Type::MouseMove}, event.motion.x * points, event.motion.y * points);
        return;
    case SDL_EVENT_MOUSE_WHEEL:
        deliver(entry, {.type = platform::Event::Type::MouseScroll, .scroll = {event.wheel.x, event.wheel.y}}, event.wheel.mouse_x * points, event.wheel.mouse_y * points);
        return;
    default:
        return;
    }
}

void OutputWindows::deliver(const Entry& entry, platform::Event event, float x, float y) {
    event.output = entry.number;
    event.position = {x, y};
    Player::handleEvent(event);
}

} // namespace haylen::platform::sdl
