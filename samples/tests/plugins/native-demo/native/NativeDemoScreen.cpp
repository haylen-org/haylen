// The screens of Windows and Linux. On Windows a screen is a window that the window of the app owns, created on the frame thread, whose messages the message loop of the app dispatches, and which disables the window of the app while it shows. On Linux it is an X11 window with a connection of its own, transient for the window of the app and marked as its modal dialog, whose events a thread of the library reads.

#include "NativeDemoScreen.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <mutex>
#include <thread>
#endif

#if defined(_WIN32)

struct NativeDemoScreen::Showing {
    static constexpr int kConfirm = 1;
    static constexpr int kDecline = 2;
    static constexpr int kWidth = 440;
    static constexpr int kHeight = 200;

    const HaylenNativeApi* api = nullptr;
    std::uint64_t screen = 0;
    HWND window = nullptr;
    HWND owner = nullptr;
    Content content;

    // Gives the window of the app back its input before the window of the screen goes, so Windows activates the window of the app again.
    void end(int ok, const std::string& json) {
        HWND ending = window;
        if (ending == nullptr) {
            return;
        }
        window = nullptr;
        EnableWindow(owner, TRUE);
        DestroyWindow(ending);
        api->finishScreen(screen, ok, json.c_str(), nullptr, 0);
    }

    static LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
        Showing& showing = getShowing();
        if (message == WM_COMMAND && LOWORD(wParam) == kConfirm) {
            if (showing.content.onConfirm) {
                showing.content.onConfirm();
            }
            showing.end(1, showing.content.confirmed);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wParam) == kDecline) {
            showing.end(showing.content.declined ? 1 : 0, showing.content.declined.value_or(showing.content.closed));
            return 0;
        }
        if (message == WM_CLOSE) {
            showing.end(0, showing.content.closed);
            return 0;
        }
        return DefWindowProcW(handle, message, wParam, lParam);
    }

    static std::wstring wide(const std::string& text) {
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (length <= 0) {
            return {};
        }
        std::wstring converted(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, converted.data(), length);
        converted.pop_back();
        return converted;
    }
};

NativeDemoScreen::Showing& NativeDemoScreen::getShowing() {
    static Showing* showing = new Showing();
    return *showing;
}

void NativeDemoScreen::open(const HaylenNativeApi& api, std::uint64_t screen, Content content) {
    HaylenNativeWindow window{};
    if (!api.getWindow(&window) || window.handle == nullptr) {
        api.finishScreen(screen, 0, R"({"message":"The app has no window yet.","code":"noWindow"})", nullptr, 0);
        return;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.lpfnWndProc = &Showing::procedure;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    type.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    type.lpszClassName = L"NativeDemoScreen";
    RegisterClassW(&type);

    // The window opens centered over the window of the app, which owns it, so it stays above the app and goes with it.
    HWND owner = static_cast<HWND>(window.handle);
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int left = (ownerRect.left + ownerRect.right - Showing::kWidth) / 2;
    const int top = (ownerRect.top + ownerRect.bottom - Showing::kHeight) / 2;
    HWND created = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NativeDemoScreen", Showing::wide(content.title).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU, left, top, Showing::kWidth, Showing::kHeight, owner, nullptr, instance, nullptr);
    CreateWindowExW(0, L"STATIC", Showing::wide(content.question).c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 24, Showing::kWidth - 40, 48, created, nullptr, instance, nullptr);
    if (content.declineLabel) {
        CreateWindowExW(0, L"BUTTON", Showing::wide(*content.declineLabel).c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP, Showing::kWidth / 2 - 150, 96, 140, 32, created, reinterpret_cast<HMENU>(static_cast<INT_PTR>(Showing::kDecline)), instance, nullptr);
    }
    const int confirmLeft = content.declineLabel ? Showing::kWidth / 2 + 10 : Showing::kWidth / 2 - 70;
    CreateWindowExW(0, L"BUTTON", Showing::wide(content.confirmLabel).c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, confirmLeft, 96, 140, 32, created, reinterpret_cast<HMENU>(static_cast<INT_PTR>(Showing::kConfirm)), instance, nullptr);

    Showing& showing = getShowing();
    showing.api = &api;
    showing.screen = screen;
    showing.window = created;
    showing.owner = owner;
    showing.content = std::move(content);
    EnableWindow(owner, FALSE);
    ShowWindow(created, SW_SHOW);
}

void NativeDemoScreen::cancel(std::uint64_t screen) {
    Showing& showing = getShowing();
    if (showing.window != nullptr && showing.screen == screen) {
        showing.end(0, R"({"message":"The app gave the screen up.","code":"cancelled"})");
    }
}

#else

// The screen that shows and whether the app gave it up, which the frame thread sets and the thread of the window reads under the lock.
struct NativeDemoScreen::Showing {
    static constexpr int kWidth = 440;
    static constexpr int kHeight = 200;
    static constexpr int kButton = 140;
    static constexpr int kDeclineLeft = 70;
    static constexpr std::string_view kGivenUp = R"({"message":"The app gave the screen up.","code":"cancelled"})";

    std::mutex lock;
    std::uint64_t screen = 0;
    bool abandoned = false;

    // The left edge of Confirm, at the right of Decline when the screen has one, and alone in the middle otherwise.
    static int confirmLeft(const Content& content) {
        return content.declineLabel ? 230 : (kWidth - kButton) / 2;
    }

    static bool inside(int x, int y, int left) {
        return x >= left && x < left + kButton && y >= 110 && y < 142;
    }

    static void drawButton(Display* display, Window window, GC context, int left, const std::string& label) {
        XDrawRectangle(display, window, context, left, 110, kButton, 32);
        XDrawString(display, window, context, left + 12, 131, label.c_str(), static_cast<int>(label.size()));
    }

    static void draw(Display* display, Window window, GC context, const Content& content) {
        XClearWindow(display, window);
        XDrawString(display, window, context, 24, 40, content.title.c_str(), static_cast<int>(content.title.size()));
        XDrawString(display, window, context, 24, 72, content.question.c_str(), static_cast<int>(content.question.size()));
        if (content.declineLabel) {
            drawButton(display, window, context, kDeclineLeft, *content.declineLabel);
        }
        drawButton(display, window, context, confirmLeft(content), content.confirmLabel);
    }

    bool isAbandoned() {
        const std::scoped_lock guard(lock);
        return abandoned;
    }

    // Runs the window on a connection of its own until the person answers, closes it or the app gives it up, and ends the screen from this thread.
    void run(const HaylenNativeApi& api, std::uint64_t id, Window owner, const Content& content) {
        Display* display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            api.finishScreen(id, 0, R"({"message":"The screen could not reach the X server.","code":"noWindow"})", nullptr, 0);
            return;
        }
        const int number = DefaultScreen(display);
        Window window = XCreateSimpleWindow(display, RootWindow(display, number), 0, 0, kWidth, kHeight, 1, BlackPixel(display, number), WhitePixel(display, number));
        XStoreName(display, window, content.title.c_str());
        XSetTransientForHint(display, window, owner);
        Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
        Atom dialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
        XChangeProperty(display, window, type, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&dialog), 1);
        Atom state = XInternAtom(display, "_NET_WM_STATE", False);
        Atom modal = XInternAtom(display, "_NET_WM_STATE_MODAL", False);
        XChangeProperty(display, window, state, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&modal), 1);
        Atom deleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &deleteWindow, 1);
        XSelectInput(display, window, ExposureMask | ButtonPressMask);
        XMapRaised(display, window);
        GC graphics = XCreateGC(display, window, 0, nullptr);
        XSetForeground(display, graphics, BlackPixel(display, number));

        std::optional<std::string> ending;
        int ok = 0;
        while (!ending) {
            if (isAbandoned()) {
                ending = std::string(kGivenUp);
                break;
            }

            while (XPending(display) > 0 && !ending) {
                XEvent event;
                XNextEvent(display, &event);
                const bool pressed = event.type == ButtonPress;
                if (event.type == Expose) {
                    draw(display, window, graphics, content);
                } else if (pressed && inside(event.xbutton.x, event.xbutton.y, confirmLeft(content))) {
                    if (content.onConfirm) {
                        content.onConfirm();
                    }
                    ending = content.confirmed;
                    ok = 1;
                } else if (pressed && content.declineLabel && inside(event.xbutton.x, event.xbutton.y, kDeclineLeft)) {
                    ok = content.declined ? 1 : 0;
                    ending = content.declined.value_or(content.closed);
                } else if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == deleteWindow) {
                    ending = content.closed;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }

        XFreeGC(display, graphics);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        {
            const std::scoped_lock guard(lock);
            screen = 0;
        }
        api.finishScreen(id, ok, ending->c_str(), nullptr, 0);
    }
};

NativeDemoScreen::Showing& NativeDemoScreen::getShowing() {
    static Showing* showing = new Showing();
    return *showing;
}

void NativeDemoScreen::open(const HaylenNativeApi& api, std::uint64_t screen, Content content) {
    HaylenNativeWindow window{};
    if (!api.getWindow(&window) || window.handle == nullptr) {
        api.finishScreen(screen, 0, R"({"message":"The app has no window yet.","code":"noWindow"})", nullptr, 0);
        return;
    }

    Showing& showing = getShowing();
    {
        const std::scoped_lock guard(showing.lock);
        showing.screen = screen;
        showing.abandoned = false;
    }
    const auto owner = static_cast<Window>(reinterpret_cast<std::uintptr_t>(window.handle));
    // clang-format off
    std::thread([&api, &showing, screen, owner, content = std::move(content)] {
        showing.run(api, screen, owner, content);
    }).detach();
    // clang-format on
}

void NativeDemoScreen::cancel(std::uint64_t screen) {
    Showing& showing = getShowing();
    const std::scoped_lock guard(showing.lock);
    if (showing.screen == screen) {
        showing.abandoned = true;
    }
}

#endif
