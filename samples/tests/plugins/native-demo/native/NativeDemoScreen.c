// The screens of Windows and Linux. On Windows a screen is a window that the window of the app owns, created on the frame thread, whose messages the message loop of the app dispatches, and which disables the window of the app while it shows. On Linux it is an X11 window with a connection of its own, transient for the window of the app and marked as its modal dialog, whose events a thread of the library reads.

#include "NativeDemoScreen.h"

#include <stdio.h>
#include <string.h>

enum { NATIVE_DEMO_SCREEN_TEXT = 512, NATIVE_DEMO_SCREEN_RESULT = 1024 };

static const char* const nativeDemoScreenGivenUp = "{\"message\":\"The app gave the screen up.\",\"code\":\"cancelled\"}";

// The copies of the content of the screen that shows, whose window outlives the call that opened it.
typedef struct NativeDemoScreenCopy {
    char title[NATIVE_DEMO_SCREEN_TEXT];
    char question[NATIVE_DEMO_SCREEN_TEXT];
    char confirmLabel[64];
    char declineLabel[64];
    char confirmed[NATIVE_DEMO_SCREEN_RESULT];
    char declined[NATIVE_DEMO_SCREEN_RESULT];
    char closed[NATIVE_DEMO_SCREEN_TEXT];
    int declines;
    void (*onConfirm)(void);
} NativeDemoScreenCopy;

static void native_demo_screen_copy(NativeDemoScreenCopy* copy, const NativeDemoScreenContent* content) {
    snprintf(copy->title, sizeof(copy->title), "%s", content->title);
    snprintf(copy->question, sizeof(copy->question), "%s", content->question);
    snprintf(copy->confirmLabel, sizeof(copy->confirmLabel), "%s", content->confirmLabel);
    snprintf(copy->declineLabel, sizeof(copy->declineLabel), "%s", content->declineLabel != NULL ? content->declineLabel : "");
    snprintf(copy->confirmed, sizeof(copy->confirmed), "%s", content->confirmed);
    snprintf(copy->declined, sizeof(copy->declined), "%s", content->declined != NULL ? content->declined : "");
    snprintf(copy->closed, sizeof(copy->closed), "%s", content->closed);
    copy->declines = content->declineLabel != NULL;
    copy->onConfirm = content->onConfirm;
}

#if defined(_WIN32)

#include <windows.h>

enum { NATIVE_DEMO_SCREEN_CONFIRM = 1, NATIVE_DEMO_SCREEN_DECLINE = 2 };

static const HaylenNativeApi* nativeDemoScreenApi = NULL;
static uint64_t nativeDemoScreenId = 0;
static HWND nativeDemoScreenWindow = NULL;
static HWND nativeDemoScreenOwner = NULL;
static NativeDemoScreenCopy nativeDemoScreenContent;

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
    const NativeDemoScreenCopy* content = &nativeDemoScreenContent;
    if (message == WM_COMMAND && LOWORD(wParam) == NATIVE_DEMO_SCREEN_CONFIRM) {
        if (content->onConfirm != NULL) {
            content->onConfirm();
        }
        native_demo_screen_end(1, content->confirmed);
        return 0;
    }
    if (message == WM_COMMAND && LOWORD(wParam) == NATIVE_DEMO_SCREEN_DECLINE) {
        native_demo_screen_end(content->declined[0] != '\0', content->declined[0] != '\0' ? content->declined : content->closed);
        return 0;
    }
    if (message == WM_CLOSE) {
        native_demo_screen_end(0, content->closed);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void native_demo_screen_wide(const char* text, wchar_t* wide) {
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, NATIVE_DEMO_SCREEN_TEXT) == 0) {
        wide[0] = L'\0';
    }
}

void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const NativeDemoScreenContent* content) {
    HaylenNativeWindow window;
    if (!api->getWindow(&window) || window.handle == NULL) {
        api->finishScreen(screen, 0, "{\"message\":\"The app has no window yet.\",\"code\":\"noWindow\"}", NULL, 0);
        return;
    }
    native_demo_screen_copy(&nativeDemoScreenContent, content);
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
    wchar_t confirmLabel[NATIVE_DEMO_SCREEN_TEXT];
    wchar_t declineLabel[NATIVE_DEMO_SCREEN_TEXT];
    native_demo_screen_wide(nativeDemoScreenContent.title, caption);
    native_demo_screen_wide(nativeDemoScreenContent.question, text);
    native_demo_screen_wide(nativeDemoScreenContent.confirmLabel, confirmLabel);
    native_demo_screen_wide(nativeDemoScreenContent.declineLabel, declineLabel);
    RECT owner;
    GetWindowRect((HWND)window.handle, &owner);
    const int width = 440;
    const int height = 200;
    HWND created = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NativeDemoScreen", caption, WS_POPUP | WS_CAPTION | WS_SYSMENU, (owner.left + owner.right - width) / 2, (owner.top + owner.bottom - height) / 2, width, height, (HWND)window.handle, NULL, instance, NULL);
    CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 24, width - 40, 48, created, NULL, instance, NULL);
    if (nativeDemoScreenContent.declines) {
        CreateWindowExW(0, L"BUTTON", declineLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP, width / 2 - 150, 96, 140, 32, created, (HMENU)(INT_PTR)NATIVE_DEMO_SCREEN_DECLINE, instance, NULL);
    }
    CreateWindowExW(0, L"BUTTON", confirmLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, nativeDemoScreenContent.declines ? width / 2 + 10 : width / 2 - 70, 96, 140, 32, created, (HMENU)(INT_PTR)NATIVE_DEMO_SCREEN_CONFIRM, instance, NULL);

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

enum { NATIVE_DEMO_SCREEN_WIDTH = 440, NATIVE_DEMO_SCREEN_HEIGHT = 200, NATIVE_DEMO_SCREEN_BUTTON = 140 };

// What the thread of the window needs: the engine, the screen, the X11 id of the window of the app and the content.
typedef struct NativeDemoScreenJob {
    const HaylenNativeApi* api;
    uint64_t screen;
    unsigned long owner;
    NativeDemoScreenCopy content;
} NativeDemoScreenJob;

// The screen that shows and whether the app gave it up, which the frame thread sets and the thread of the window reads.
static pthread_mutex_t nativeDemoScreenLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t nativeDemoScreenShowing = 0;
static int nativeDemoScreenAbandoned = 0;

// The left edges of the buttons, Decline at the left of Confirm when the screen has one, and Confirm alone in the middle otherwise.
static int native_demo_screen_confirm_left(const NativeDemoScreenCopy* content) {
    return content->declines ? 230 : (NATIVE_DEMO_SCREEN_WIDTH - NATIVE_DEMO_SCREEN_BUTTON) / 2;
}

static int native_demo_screen_inside(int x, int y, int left) {
    return x >= left && x < left + NATIVE_DEMO_SCREEN_BUTTON && y >= 110 && y < 142;
}

static void native_demo_screen_button(Display* display, Window window, GC context, int left, const char* label) {
    XDrawRectangle(display, window, context, left, 110, NATIVE_DEMO_SCREEN_BUTTON, 32);
    XDrawString(display, window, context, left + 12, 131, label, (int)strlen(label));
}

static void native_demo_screen_draw(Display* display, Window window, GC context, const NativeDemoScreenCopy* content) {
    XClearWindow(display, window);
    XDrawString(display, window, context, 24, 40, content->title, (int)strlen(content->title));
    XDrawString(display, window, context, 24, 72, content->question, (int)strlen(content->question));
    if (content->declines) {
        native_demo_screen_button(display, window, context, 70, content->declineLabel);
    }
    native_demo_screen_button(display, window, context, native_demo_screen_confirm_left(content), content->confirmLabel);
}

// Runs the window on a connection of its own until the person answers, closes it or the app gives it up, and ends the screen from this thread.
static void* native_demo_screen_run(void* context) {
    NativeDemoScreenJob* job = (NativeDemoScreenJob*)context;
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        job->api->finishScreen(job->screen, 0, "{\"message\":\"The screen could not reach the X server.\",\"code\":\"noWindow\"}", NULL, 0);
        free(job);
        return NULL;
    }
    const int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, NATIVE_DEMO_SCREEN_WIDTH, NATIVE_DEMO_SCREEN_HEIGHT, 1, BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, job->content.title);
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

    const NativeDemoScreenCopy* content = &job->content;
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
                native_demo_screen_draw(display, window, graphics, content);
            } else if (event.type == ButtonPress && native_demo_screen_inside(event.xbutton.x, event.xbutton.y, native_demo_screen_confirm_left(content))) {
                if (content->onConfirm != NULL) {
                    content->onConfirm();
                }
                ending = content->confirmed;
                ok = 1;
            } else if (event.type == ButtonPress && content->declines && native_demo_screen_inside(event.xbutton.x, event.xbutton.y, 70)) {
                ok = content->declined[0] != '\0';
                ending = ok ? content->declined : content->closed;
            } else if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == deleteWindow) {
                ending = content->closed;
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

void native_demo_screen_open(const HaylenNativeApi* api, uint64_t screen, const NativeDemoScreenContent* content) {
    HaylenNativeWindow window;
    if (!api->getWindow(&window) || window.handle == NULL) {
        api->finishScreen(screen, 0, "{\"message\":\"The app has no window yet.\",\"code\":\"noWindow\"}", NULL, 0);
        return;
    }
    NativeDemoScreenJob* job = (NativeDemoScreenJob*)calloc(1, sizeof(NativeDemoScreenJob));
    job->api = api;
    job->screen = screen;
    job->owner = (unsigned long)(uintptr_t)window.handle;
    native_demo_screen_copy(&job->content, content);
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
