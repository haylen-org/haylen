#include "platform/linux/LinuxDesktop.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

#include "haylen/core/Log.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/NativeWindow.hpp"
#include "platform/Player.hpp"
#include "platform/linux/LinuxVisualColor.hpp"

// Xlib defines macros such as `None` and `Bool`, so it comes after the engine headers that use those names.
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>

namespace haylen::platform {

::Display* LinuxDesktop::watcher = nullptr;
unsigned long LinuxDesktop::workAreaAtom = 0;
int LinuxDesktop::randrEvents = 0;
WindowStyle LinuxDesktop::style{};
Window::Passthrough LinuxDesktop::passthrough = Window::Passthrough::Off;
std::vector<math::Polygon::Outline>& LinuxDesktop::regions = *new std::vector<math::Polygon::Outline>();
bool LinuxDesktop::dragging = false;

::Display* LinuxDesktop::getDisplay() {
    return static_cast<::Display*>(NativeWindow::getDisplay());
}

::Window LinuxDesktop::getWindow() {
    return static_cast<::Window>(reinterpret_cast<std::uintptr_t>(NativeWindow::getHandle()));
}

// Desktop points follow `Xft.dpi`, the scale that the toolkits of the desktop share, and a desktop without it has the standard 96 DPI.
float LinuxDesktop::getDesktopScale(::Display* display) {
    constexpr float kStandardDpi = 96.0F;
    XrmInitialize();
    const char* resources = XResourceManagerString(display);
    XrmDatabase database = resources != nullptr ? XrmGetStringDatabase(resources) : nullptr;
    if (database == nullptr) {
        return 1.0F;
    }
    float dpi = kStandardDpi;
    XrmValue value{};
    char* type = nullptr;
    if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) == True && type != nullptr && std::string_view(type) == "String") {
        dpi = std::strtof(value.addr, nullptr);
    }
    XrmDestroyDatabase(database);
    return dpi > 0.0F ? dpi / kStandardDpi : 1.0F;
}

std::vector<long> LinuxDesktop::readCardinals(::Display* display, ::Window window, const char* name) {
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(display, window, XInternAtom(display, name, False), 0, 1024, False, XA_CARDINAL, &type, &format, &count, &remaining, &data);
    std::vector<long> values;
    if (status == Success && data != nullptr && format == 32) {
        const auto* cardinals = reinterpret_cast<const long*>(data);
        values.assign(cardinals, cardinals + count);
    }
    if (data != nullptr) {
        XFree(data);
    }
    return values;
}

// The window manager publishes one work area for the whole screen on every virtual desktop, and the current desktop picks its own.
math::Rect LinuxDesktop::readWorkArea(::Display* display, float scale) {
    const ::Window root = DefaultRootWindow(display);
    const std::vector<long> current = readCardinals(display, root, "_NET_CURRENT_DESKTOP");
    const std::vector<long> areas = readCardinals(display, root, "_NET_WORKAREA");
    const std::size_t desktop = current.empty() ? 0 : static_cast<std::size_t>(current.front());
    if (areas.size() < (desktop + 1) * 4) {
        return {};
    }
    const long* area = areas.data() + desktop * 4;
    return {static_cast<float>(area[0]) / scale, static_cast<float>(area[1]) / scale, static_cast<float>(area[2]) / scale, static_cast<float>(area[3]) / scale};
}

// The rate of a mode is its pixel clock over the pixels of a whole frame, the blanking included, which an interlaced mode draws twice and a double scanned one once in two.
float LinuxDesktop::getRefreshRate(const XRRModeInfo& mode) {
    if (mode.hTotal == 0 || mode.vTotal == 0) {
        return 0.0F;
    }
    double rate = static_cast<double>(mode.dotClock) / (static_cast<double>(mode.hTotal) * static_cast<double>(mode.vTotal));
    if ((mode.modeFlags & RR_Interlace) != 0) {
        rate *= 2.0;
    }
    if ((mode.modeFlags & RR_DoubleScan) != 0) {
        rate /= 2.0;
    }
    return static_cast<float>(rate);
}

// A monitor of RandR shows one output or more, and the first one names it, with the name of its connector, such as `DP-1`, which stays the same across runs and reconnections, its modes and the rate of the mode it shows.
void LinuxDesktop::describeOutput(::Display* display, XRRScreenResources* resources, RROutput output, platform::Display& described) {
    XRROutputInfo* info = XRRGetOutputInfo(display, resources, output);
    if (info == nullptr) {
        return;
    }
    described.id.assign(info->name, static_cast<std::size_t>(info->nameLen));
    RRMode current = None;
    if (XRRCrtcInfo* crtc = info->crtc != None ? XRRGetCrtcInfo(display, resources, info->crtc) : nullptr) {
        current = crtc->mode;
        XRRFreeCrtcInfo(crtc);
    }

    for (int index = 0; index < info->nmode; ++index) {
        for (int known = 0; known < resources->nmode; ++known) {
            const XRRModeInfo& mode = resources->modes[known];
            if (mode.id != info->modes[index]) {
                continue;
            }
            const platform::Display::Mode found{.width = static_cast<int>(mode.width), .height = static_cast<int>(mode.height), .refreshRate = getRefreshRate(mode), .scale = described.scale};
            if (std::ranges::find(described.modes, found) == described.modes.end()) {
                described.modes.push_back(found);
            }
            if (mode.id == current) {
                described.refreshRate = found.refreshRate;
            }
        }
    }
    XRRFreeOutputInfo(info);
}

std::vector<platform::Display> LinuxDesktop::readDisplays(::Display* display) {
    const float scale = getDesktopScale(display);
    const math::Rect workArea = readWorkArea(display, scale);
    const auto toPoints = [scale](int x, int y, int width, int height) { return math::Rect{static_cast<float>(x) / scale, static_cast<float>(y) / scale, static_cast<float>(width) / scale, static_cast<float>(height) / scale}; };
    const auto clip = [&workArea](const math::Rect& bounds) { return workArea.intersects(bounds) ? workArea.intersection(bounds) : bounds; };

    std::vector<platform::Display> displays;
    const ::Window root = DefaultRootWindow(display);
    XRRScreenResources* resources = XRRGetScreenResourcesCurrent(display, root);
    int count = 0;
    XRRMonitorInfo* infos = XRRGetMonitors(display, root, True, &count);
    for (int index = 0; index < count; ++index) {
        const XRRMonitorInfo& info = infos[index];
        char* name = XGetAtomName(display, info.name);
        const math::Rect bounds = toPoints(info.x, info.y, info.width, info.height);
        platform::Display& added = displays.emplace_back(platform::Display{.id = name != nullptr ? name : "", .name = name != nullptr ? name : "", .bounds = bounds, .workArea = clip(bounds), .scale = scale, .primary = info.primary != 0});
        XFree(name);
        if (resources != nullptr && info.noutput > 0) {
            describeOutput(display, resources, info.outputs[0], added);
        }
    }
    if (infos != nullptr) {
        XRRFreeMonitors(infos);
    }
    if (resources != nullptr) {
        XRRFreeScreenResources(resources);
    }

    // Servers that report no monitors through RandR, such as some virtual displays, have their whole screen as the only display.
    if (displays.empty()) {
        const int screen = DefaultScreen(display);
        const math::Rect bounds = toPoints(0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen));
        displays.push_back({.id = "screen", .name = "screen", .bounds = bounds, .workArea = clip(bounds), .scale = scale, .primary = true});
    }
    return displays;
}

// The player places the window before SDL connects to the display, so it reads the displays through a connection of its own then.
std::vector<platform::Display> LinuxDesktop::getDisplays() {
    if (::Display* display = getDisplay()) {
        return readDisplays(display);
    }
    ::Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        throw std::runtime_error("The X11 display could not be opened to read its displays.");
    }
    std::vector<platform::Display> displays = readDisplays(display);
    XCloseDisplay(display);
    return displays;
}

std::string LinuxDesktop::getWindowDisplayId() {
    const std::vector<platform::Display> displays = getDisplays();
    return platform::Display::findHolding(displays, getFrame()).id;
}

// Window managers change the state of a mapped window when it asks the root window.
void LinuxDesktop::changeState(::Display* display, ::Window window, const char* state, bool enabled) {
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = enabled ? 1 : 0;
    event.xclient.data.l[1] = static_cast<long>(XInternAtom(display, state, False));
    event.xclient.data.l[3] = kFromApplication;
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

// Static gravity places frames by their content area, and a window whose smallest and largest sizes match keeps that size.
void LinuxDesktop::applySizeHints(::Display* display, ::Window window, int width, int height) {
    XSizeHints hints{};
    hints.flags = PWinGravity;
    hints.win_gravity = StaticGravity;
    if (!style.resizable) {
        hints.flags |= PMinSize | PMaxSize;
        hints.min_width = hints.max_width = width;
        hints.min_height = hints.max_height = height;
    }
    XSetWMNormalHints(display, window, &hints);
}

// The server fills a window with its background as it maps it, before the app presented a frame into it, and with or without a compositor it keeps nothing that the app presented while the window was hidden. A window of a visual of another class than true color keeps its background. A window on Wayland, which shows only the frames of the app, has none.
void LinuxDesktop::setBackground(const math::Color& value) {
    ::Display* display = getDisplay();
    if (display == nullptr) {
        return;
    }
    const ::Window window = getWindow();
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(display, window, &attributes) == 0 || attributes.visual == nullptr || attributes.visual->c_class != TrueColor) {
        return;
    }
    const Visual& visual = *attributes.visual;
    XSetWindowBackground(display, window, LinuxVisualColor::toPixel(value, visual.red_mask, visual.green_mask, visual.blue_mask));
    XFlush(display);
}

// Motif hints are how X11 apps ask window managers for a window without decorations.
void LinuxDesktop::setStyle(const WindowStyle& value) {
    style = value;
    ::Display* display = getDisplay();
    if (display == nullptr) {
        return;
    }
    const ::Window window = getWindow();
    const Atom motif = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    const std::array<long, 5> hints{kMotifDecorations, 0, value.decorated ? 1 : 0, 0, 0};
    XChangeProperty(display, window, motif, motif, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(hints.data()), static_cast<int>(hints.size()));
    changeState(display, window, "_NET_WM_STATE_ABOVE", value.alwaysOnTop);
    changeState(display, window, "_NET_WM_STATE_SKIP_TASKBAR", !value.shownInTaskbar);
    changeState(display, window, "_NET_WM_STATE_SKIP_PAGER", !value.shownInTaskbar);
    const math::Vec2 size = NativeWindow::getFramebufferSize() / NativeWindow::getScale();
    applySizeHints(display, window, static_cast<int>(std::lround(size.x)), static_cast<int>(std::lround(size.y)));
    XFlush(display);
}

// The scale of the window turns its framebuffer into its pixels, which are the units of X11.
math::Rect LinuxDesktop::getFrame() {
    const math::Vec2 pixels = NativeWindow::getFramebufferSize() / NativeWindow::getScale();
    ::Display* display = getDisplay();
    if (display == nullptr) {
        return {0.0F, 0.0F, pixels.x, pixels.y};
    }
    int x = 0;
    int y = 0;
    ::Window child = 0;
    XTranslateCoordinates(display, getWindow(), DefaultRootWindow(display), 0, 0, &x, &y, &child);
    const float scale = getDesktopScale(display);
    return {static_cast<float>(x) / scale, static_cast<float>(y) / scale, pixels.x / scale, pixels.y / scale};
}

std::string LinuxDesktop::getTitle() {
    ::Display* display = getDisplay();
    char* name = nullptr;
    if (display == nullptr || XFetchName(display, getWindow(), &name) == 0 || name == nullptr) {
        return {};
    }
    std::string title(name);
    XFree(name);
    return title;
}

math::Vec2 LinuxDesktop::getOrigin() {
    ::Display* display = getDisplay();
    if (display == nullptr) {
        return {};
    }
    int x = 0;
    int y = 0;
    ::Window child = 0;
    XTranslateCoordinates(display, getWindow(), DefaultRootWindow(display), 0, 0, &x, &y, &child);
    return {static_cast<float>(x), static_cast<float>(y)};
}

void LinuxDesktop::setFrame(const math::Rect& value) {
    ::Display* display = getDisplay();
    if (display == nullptr) {
        return;
    }
    const ::Window window = getWindow();
    const float scale = getDesktopScale(display);
    const auto width = static_cast<int>(std::lround(value.width * scale));
    const auto height = static_cast<int>(std::lround(value.height * scale));
    applySizeHints(display, window, width, height);
    XMoveResizeWindow(display, window, static_cast<int>(std::lround(value.x * scale)), static_cast<int>(std::lround(value.y * scale)), static_cast<unsigned int>(width), static_cast<unsigned int>(height));
    XFlush(display);
}

// The window takes the mouse only inside its input shape, which changes only when the regions do, since apps may give them every frame.
void LinuxDesktop::setPassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> value) {
    if (mode == passthrough && std::ranges::equal(value, regions)) {
        return;
    }
    passthrough = mode;
    regions.assign(value.begin(), value.end());

    ::Display* display = getDisplay();
    if (display == nullptr) {
        return;
    }
    const ::Window window = getWindow();
    if (mode == Window::Passthrough::Off) {
        XShapeCombineMask(display, window, ShapeInput, 0, 0, None, ShapeSet);
        XFlush(display);
        return;
    }

    // An empty shape lets every click through the whole window, and the regions in framebuffer pixels shape it in the pixels of the window.
    Region shape = XCreateRegion();
    if (mode == Window::Passthrough::Regions) {
        const float scale = NativeWindow::getScale();
        for (const math::Polygon::Outline& region : regions) {
            std::vector<XPoint> points;
            points.reserve(region.size());
            for (const math::Vec2 point : region) {
                points.push_back({.x = static_cast<short>(std::lround(point.x / scale)), .y = static_cast<short>(std::lround(point.y / scale))});
            }
            Region polygon = XPolygonRegion(points.data(), static_cast<int>(points.size()), WindingRule);
            XUnionRegion(shape, polygon, shape);
            XDestroyRegion(polygon);
        }
    }
    XShapeCombineRegion(display, window, ShapeInput, 0, 0, shape, ShapeSet);
    XDestroyRegion(shape);
    XFlush(display);
}

// The window manager moves the window once the grab of the press ends, and keeps the release of the button from the app.
void LinuxDesktop::startDrag() {
    ::Display* display = getDisplay();
    if (display == nullptr) {
        return;
    }
    const ::Window window = getWindow();
    ::Window root = 0;
    ::Window child = 0;
    int rootX = 0;
    int rootY = 0;
    int x = 0;
    int y = 0;
    unsigned int buttons = 0;
    XQueryPointer(display, window, &root, &child, &rootX, &rootY, &x, &y, &buttons);
    if ((buttons & Button1Mask) == 0) {
        return;
    }

    XUngrabPointer(display, CurrentTime);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;
    event.xclient.data.l[1] = rootY;
    event.xclient.data.l[2] = kMoveWindow;
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = kFromApplication;
    XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    dragging = true;
}

// The event loop of SDL drops events it does not handle, so a connection of its own hears the configure events of the window, the work area and RandR.
void LinuxDesktop::watch() {
    if (getDisplay() == nullptr) {
        return;
    }
    watcher = XOpenDisplay(nullptr);
    if (watcher == nullptr) {
        core::Log::error("A second connection to the X11 display could not be opened, so the app will not hear window moves and display changes.");
        return;
    }
    const ::Window root = DefaultRootWindow(watcher);
    XSelectInput(watcher, getWindow(), StructureNotifyMask);
    XSelectInput(watcher, root, PropertyChangeMask);
    workAreaAtom = XInternAtom(watcher, "_NET_WORKAREA", False);
    int errors = 0;
    if (XRRQueryExtension(watcher, &randrEvents, &errors) == True) {
        XRRSelectInput(watcher, root, RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask | RROutputChangeNotifyMask);
    }
    XFlush(watcher);
}

void LinuxDesktop::update() {
    if (dragging) {
        followDrag(getDisplay());
    }
    if (watcher != nullptr) {
        readEvents();
    }
}

void LinuxDesktop::followDrag(::Display* display) {
    ::Window root = 0;
    ::Window child = 0;
    int rootX = 0;
    int rootY = 0;
    int x = 0;
    int y = 0;
    unsigned int buttons = 0;
    XQueryPointer(display, getWindow(), &root, &child, &rootX, &rootY, &x, &y, &buttons);
    if ((buttons & Button1Mask) != 0) {
        return;
    }
    dragging = false;
    Player::handleEvent({.type = Event::Type::MouseUp, .mouseButton = input::MouseButton::Left, .position = {static_cast<float>(x), static_cast<float>(y)}});
}

void LinuxDesktop::readEvents() {
    while (XPending(watcher) > 0) {
        XEvent event{};
        XNextEvent(watcher, &event);
        const bool randr = randrEvents > 0 && (event.type == randrEvents + RRScreenChangeNotify || event.type == randrEvents + RRNotify);
        if (randr) {
            XRRUpdateConfiguration(&event);
        }
        if (event.type == ConfigureNotify) {
            Player::handleEvent({.type = Event::Type::WindowMoved});
        } else if (randr || (event.type == PropertyNotify && event.xproperty.atom == workAreaAtom)) {
            Player::handleEvent({.type = Event::Type::DisplaysChanged});
        }
    }
}

void LinuxDesktop::release() noexcept {
    if (watcher != nullptr) {
        XCloseDisplay(watcher);
        watcher = nullptr;
    }
}

} // namespace haylen::platform
