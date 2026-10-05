// The confirm screen of Windows and Linux. On Windows it is a window that the window of the app owns, created on the frame thread, whose messages the message loop of the app dispatches, and which disables the window of the app while it shows. On Linux it is an X11 window with a connection of its own, transient for the window of the app and marked as its modal dialog, whose events a thread of the library reads.

#include "NativeDemoScreen.h"

#include <stdio.h>
#include <string.h>

static const char* const nativeDemoScreenConfirmed = "{\"confirmed\":true,\"via\":\"a window over the window of the app\",\"language\":\"C\"}";
static const char* const nativeDemoScreenDeclined = "{\"confirmed\":false,\"via\":\"a window over the window of the app\",\"language\":\"C\"}";
static const char* const nativeDemoScreenClosed = "{\"message\":\"The person closed the confirm screen.\",\"code\":\"cancelled\"}";
static const char* const nativeDemoScreenGivenUp = "{\"message\":\"The app gave the confirm screen up.\",\"code\":\"cancelled\"}";

#if defined(_WIN32)

#include <windows.h>

enum { NATIVE_DEMO_SCREEN_CONFIRM = 1, NATIVE_DEMO_SCREEN_DECLINE = 2, NATIVE_DEMO_SCREEN_TEXT = 512 };

static const HaylenNativeApi* nativeDemoScreenApi = NULL;
static uint64_t nativeDemoScreenId = 0;
static HWND nativeDemoScreenWindow = NULL;
static HWND nativeDemoScreenOwner = NULL;

// Gives the window of the app back its input before the window of the screen goes, so Windows activates the window of the app again.
static void native_demo_screen_end(int ok, const char* json) {
    HWND window = nativeDemoScreenWindow;
    if (window == NULL) {
        return;
    }
    nativeDemoScreenWindow = NULL;
    EnableWindow(nativeDemoScreenOwner, TRUE);
    DestroyWindow(window);
    nativeDemoScreenApi->finishScreen(nativeDemoScreenId, ok, json, NULL, 0);
}

static LRESULT CALLBACK native_demo_screen_procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND && (LOWORD(wParam) == NATIVE_DEMO_SCREEN_CONFIRM || LOWORD(wParam) == NATIVE_DEMO_SCREEN_DECLINE)) {
        native_demo_screen_end(1, LOWORD(wParam) == NATIVE_DEMO_SCREEN_CONFIRM ? nativeDemoScreenConfirmed : nativeDemoScreenDeclined);
        return 0;
    }
    if (message == WM_CLOSE) {
        native_demo_screen_end(0, nativeDemoScreenClosed);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void native_demo_screen_wide(const char* text, wchar_t* wide) {
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, NATIVE_DEMO_SCREEN_TEXT) == 0) {
        wide[0] = L'\0';
    }
}

void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const char* title, const char* question) {
    HaylenNativeWindow window;
    if (!api->getWindow(&window) || window.handle == NULL) {
        api->finishScreen(screen, 0, "{\"message\":\"The app has no window yet.\",\"code\":\"noWindow\"}", NULL, 0);
        return;
    }
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW type = {0};
    type.lpfnWndProc = native_demo_screen_procedure;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    type.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    type.lpszClassName = L"NativeDemoScreen";
    RegisterClassW(&type);

    // The window opens centered over the window of the app, which owns it, so it stays above the app and goes with it.
    wchar_t caption[NATIVE_DEMO_SCREEN_TEXT];
    wchar_t text[NATIVE_DEMO_SCREEN_TEXT];
    native_demo_screen_wide(title, caption);
    native_demo_screen_wide(question, text);
    RECT owner;
    GetWindowRect((HWND)window.handle, &owner);
    const int width = 440;
    const int height = 200;
    HWND created = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NativeDemoScreen", caption, WS_POPUP | WS_CAPTION | WS_SYSMENU, (owner.left + owner.right - width) / 2, (owner.top + owner.bottom - height) / 2, width, height, (HWND)window.handle, NULL, instance, NULL);
    CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 24, width - 40, 48, created, NULL, instance, NULL);
    CreateWindowExW(0, L"BUTTON", L"Decline", WS_CHILD | WS_VISIBLE | WS_TABSTOP, width / 2 - 130, 96, 110, 32, created, (HMENU)(INT_PTR)NATIVE_DEMO_SCREEN_DECLINE, instance, NULL);
    CreateWindowExW(0, L"BUTTON", L"Confirm", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, width / 2 + 10, 96, 110, 32, created, (HMENU)(INT_PTR)NATIVE_DEMO_SCREEN_CONFIRM, instance, NULL);

    nativeDemoScreenApi = api;
    nativeDemoScreenId = screen;
    nativeDemoScreenWindow = created;
    nativeDemoScreenOwner = (HWND)window.handle;
    EnableWindow(nativeDemoScreenOwner, FALSE);
    ShowWindow(created, SW_SHOW);
}

void native_demo_screen_cancel(uint64_t screen) {
    if (nativeDemoScreenWindow != NULL && nativeDemoScreenId == screen) {
        native_demo_screen_end(0, nativeDemoScreenGivenUp);
    }
}

#else

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

enum { NATIVE_DEMO_SCREEN_WIDTH = 440, NATIVE_DEMO_SCREEN_HEIGHT = 200, NATIVE_DEMO_SCREEN_TEXT = 512 };

// What the thread of the window needs: the engine, the screen, the X11 id of the window of the app and the texts.
typedef struct NativeDemoScreenJob {
    const HaylenNativeApi* api;
    uint64_t screen;
    unsigned long owner;
    char title[NATIVE_DEMO_SCREEN_TEXT];
    char question[NATIVE_DEMO_SCREEN_TEXT];
} NativeDemoScreenJob;

// The screen that shows and whether the app gave it up, which the frame thread sets and the thread of the window reads.
static pthread_mutex_t nativeDemoScreenLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t nativeDemoScreenShowing = 0;
static int nativeDemoScreenAbandoned = 0;

static int native_demo_screen_inside(int x, int y, int left) {
    return x >= left && x < left + 110 && y >= 110 && y < 142;
}

static void native_demo_screen_draw(Display* display, Window window, GC context, const NativeDemoScreenJob* job) {
    XClearWindow(display, window);
    XDrawString(display, window, context, 24, 40, job->title, (int)strlen(job->title));
    XDrawString(display, window, context, 24, 72, job->question, (int)strlen(job->question));
    XDrawRectangle(display, window, context, 90, 110, 110, 32);
    XDrawString(display, window, context, 122, 131, "Decline", 7);
    XDrawRectangle(display, window, context, 240, 110, 110, 32);
    XDrawString(display, window, context, 272, 131, "Confirm", 7);
}

// Runs the window on a connection of its own until the person answers, closes it or the app gives it up, and ends the screen from this thread.
static void* native_demo_screen_run(void* context) {
    NativeDemoScreenJob* job = (NativeDemoScreenJob*)context;
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        job->api->finishScreen(job->screen, 0, "{\"message\":\"The confirm screen could not reach the X server.\",\"code\":\"noWindow\"}", NULL, 0);
        free(job);
        return NULL;
    }
    const int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, NATIVE_DEMO_SCREEN_WIDTH, NATIVE_DEMO_SCREEN_HEIGHT, 1, BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, job->title);
    XSetTransientForHint(display, window, (Window)job->owner);
    Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom dialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(display, window, type, XA_ATOM, 32, PropModeReplace, (unsigned char*)&dialog, 1);
    Atom state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom modal = XInternAtom(display, "_NET_WM_STATE_MODAL", False);
    XChangeProperty(display, window, state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&modal, 1);
    Atom deleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &deleteWindow, 1);
    XSelectInput(display, window, ExposureMask | ButtonPressMask);
    XMapRaised(display, window);
    GC graphics = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, graphics, BlackPixel(display, screen));

    const char* ending = NULL;
    int ok = 0;
    while (ending == NULL) {
        pthread_mutex_lock(&nativeDemoScreenLock);
        const int givenUp = nativeDemoScreenAbandoned;
        pthread_mutex_unlock(&nativeDemoScreenLock);
        if (givenUp) {
            ending = nativeDemoScreenGivenUp;
            break;
        }
        while (XPending(display) > 0 && ending == NULL) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == Expose) {
                native_demo_screen_draw(display, window, graphics, job);
            } else if (event.type == ButtonPress && native_demo_screen_inside(event.xbutton.x, event.xbutton.y, 240)) {
                ending = nativeDemoScreenConfirmed;
                ok = 1;
            } else if (event.type == ButtonPress && native_demo_screen_inside(event.xbutton.x, event.xbutton.y, 90)) {
                ending = nativeDemoScreenDeclined;
                ok = 1;
            } else if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == deleteWindow) {
                ending = nativeDemoScreenClosed;
            }
        }
        const struct timespec pause = {0, 15000000L};
        nanosleep(&pause, NULL);
    }

    XFreeGC(display, graphics);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    pthread_mutex_lock(&nativeDemoScreenLock);
    nativeDemoScreenShowing = 0;
    pthread_mutex_unlock(&nativeDemoScreenLock);
    job->api->finishScreen(job->screen, ok, ending, NULL, 0);
    free(job);
    return NULL;
}

void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const char* title, const char* question) {
    HaylenNativeWindow window;
    if (!api->getWindow(&window) || window.handle == NULL) {
        api->finishScreen(screen, 0, "{\"message\":\"The app has no window yet.\",\"code\":\"noWindow\"}", NULL, 0);
        return;
    }
    NativeDemoScreenJob* job = (NativeDemoScreenJob*)calloc(1, sizeof(NativeDemoScreenJob));
    job->api = api;
    job->screen = screen;
    job->owner = (unsigned long)(uintptr_t)window.handle;
    snprintf(job->title, sizeof(job->title), "%s", title);
    snprintf(job->question, sizeof(job->question), "%s", question);
    pthread_mutex_lock(&nativeDemoScreenLock);
    nativeDemoScreenShowing = screen;
    nativeDemoScreenAbandoned = 0;
    pthread_mutex_unlock(&nativeDemoScreenLock);

    pthread_t thread;
    pthread_create(&thread, NULL, native_demo_screen_run, job);
    pthread_detach(thread);
}

void native_demo_screen_cancel(uint64_t screen) {
    pthread_mutex_lock(&nativeDemoScreenLock);
    if (nativeDemoScreenShowing == screen) {
        nativeDemoScreenAbandoned = 1;
    }
    pthread_mutex_unlock(&nativeDemoScreenLock);
}

#endif
