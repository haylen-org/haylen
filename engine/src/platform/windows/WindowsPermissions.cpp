#include "platform/windows/WindowsPermissions.hpp"

#include <roapi.h>
#include <windows.foundation.h>
#include <windows.security.authorization.appcapabilityaccess.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>

#include <format>
#include <utility>

#include "platform/windows/WindowsText.hpp"

namespace haylen::platform {

using ABI::Windows::Foundation::AsyncStatus;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Security::Authorization::AppCapabilityAccess::AppCapabilityAccessStatus;
using ABI::Windows::Security::Authorization::AppCapabilityAccess::IAppCapability;
using ABI::Windows::Security::Authorization::AppCapabilityAccess::IAppCapabilityStatics;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

bool WindowsPermissions::runtimeStarted = false;

// A Windows without `AppCapability` tells the app nothing of its privacy settings until the app opens the device.
void WindowsPermissions::check(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    const wchar_t* name = getCapabilityName(kind);
    if (name == nullptr) {
        callback(answerWithoutCapability(kind));
        return;
    }

    ComPtr<IAppCapabilityStatics> statics;
    ComPtr<IAppCapability> capability;
    AppCapabilityAccessStatus status{};
    if (!startRuntime() || FAILED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Security_Authorization_AppCapabilityAccess_AppCapability).Get(), IID_PPV_ARGS(&statics))) || FAILED(statics->Create(HStringReference(name).Get(), &capability)) || FAILED(capability->CheckAccess(&status))) {
        callback({.state = Permission::State::Unknown});
        return;
    }
    callback(answerStatus(static_cast<int>(status), name));
}

// Windows shows its prompt only while the privacy settings ask the person, such as for the location of desktop apps on recent releases, so a capability that the settings allow or refuse answers at once.
void WindowsPermissions::request(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    const wchar_t* name = getCapabilityName(kind);
    if (name == nullptr) {
        callback(answerWithoutCapability(kind));
        return;
    }

    ComPtr<IAppCapabilityStatics> statics;
    ComPtr<IAppCapability> capability;
    AppCapabilityAccessStatus status{};
    if (!startRuntime() || FAILED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Security_Authorization_AppCapabilityAccess_AppCapability).Get(), IID_PPV_ARGS(&statics))) || FAILED(statics->Create(HStringReference(name).Get(), &capability)) || FAILED(capability->CheckAccess(&status))) {
        callback({.state = Permission::State::Unknown});
        return;
    }
    if (status != AppCapabilityAccessStatus::AppCapabilityAccessStatus_UserPromptRequired) {
        callback(answerStatus(static_cast<int>(status), name));
        return;
    }

    ComPtr<IAsyncOperation<AppCapabilityAccessStatus>> operation;
    if (FAILED(capability->RequestAccessAsync(&operation))) {
        callback({.state = Permission::State::Prompt});
        return;
    }

    // clang-format off
    const HRESULT waiting = operation->put_Completed(Callback<IAsyncOperationCompletedHandler<AppCapabilityAccessStatus>>([name, callback](IAsyncOperation<AppCapabilityAccessStatus>* finished, AsyncStatus outcome) {
        AppCapabilityAccessStatus status{};
        if (outcome != AsyncStatus::Completed || FAILED(finished->GetResults(&status))) {
            callback({.state = Permission::State::Unknown});
            return S_OK;
        }
        callback(answerStatus(static_cast<int>(status), name));
        return S_OK;
    }).Get());
    // clang-format on
    if (FAILED(waiting)) {
        callback({.state = Permission::State::Unknown});
    }
}

const wchar_t* WindowsPermissions::getCapabilityName(Permission::Kind kind) noexcept {
    switch (kind) {
    case Permission::Kind::Camera:
        return L"webcam";
    case Permission::Kind::Microphone:
        return L"microphone";
    case Permission::Kind::Location:
    case Permission::Kind::PreciseLocation:
    case Permission::Kind::BackgroundLocation:
        return L"location";
    default:
        return nullptr;
    }
}

Permission::Answer WindowsPermissions::answerWithoutCapability(Permission::Kind kind) {
    return {.state = kind == Permission::Kind::Notifications ? Permission::State::Unknown : Permission::State::Granted};
}

// The values are those of `AppCapabilityAccessStatus`, whose two refusals are the setting of the person and the one of the system or its policy.
Permission::Answer WindowsPermissions::answerStatus(int status, const wchar_t* capability) {
    switch (static_cast<AppCapabilityAccessStatus>(status)) {
    case AppCapabilityAccessStatus::AppCapabilityAccessStatus_Allowed:
        return {.state = Permission::State::Granted};
    case AppCapabilityAccessStatus::AppCapabilityAccessStatus_UserPromptRequired:
        return {.state = Permission::State::Prompt};
    case AppCapabilityAccessStatus::AppCapabilityAccessStatus_DeniedByUser:
    case AppCapabilityAccessStatus::AppCapabilityAccessStatus_DeniedBySystem:
        return {.state = Permission::State::Denied};
    case AppCapabilityAccessStatus::AppCapabilityAccessStatus_NotDeclaredByApp:
        return {.state = Permission::State::Unsupported, .message = std::format("The app does not declare the capability \"{}\" of Windows.", WindowsText::toUtf8(capability))};
    }
    return {.state = Permission::State::Unknown};
}

bool WindowsPermissions::startRuntime() {
    if (!runtimeStarted) {
        const HRESULT started = RoInitialize(RO_INIT_SINGLETHREADED);
        runtimeStarted = SUCCEEDED(started) || started == RPC_E_CHANGED_MODE;
    }
    return runtimeStarted;
}

} // namespace haylen::platform
