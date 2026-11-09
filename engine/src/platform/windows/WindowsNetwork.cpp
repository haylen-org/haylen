#include "platform/windows/WindowsNetwork.hpp"

#include <ocidl.h>

#include "haylen/platform/Event.hpp"
#include "platform/Player.hpp"

namespace haylen::platform {

using Microsoft::WRL::ComPtr;

ComPtr<INetworkListManager>& WindowsNetwork::manager = *new ComPtr<INetworkListManager>();
ComPtr<IConnectionPoint>& WindowsNetwork::point = *new ComPtr<IConnectionPoint>();

// The thread of the window is a single-threaded apartment, whose messages deliver the changes. A system without the manager leaves the network state unknown.
void WindowsNetwork::observe() {
    const HRESULT started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(started) && started != RPC_E_CHANGED_MODE) {
        return;
    }
    if (FAILED(CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&manager)))) {
        return;
    }

    ComPtr<IConnectionPointContainer> container;
    if (FAILED(manager.As(&container)) || FAILED(container->FindConnectionPoint(__uuidof(INetworkListManagerEvents), &point))) {
        return;
    }
    const ComPtr<WindowsNetwork> sink = Microsoft::WRL::Make<WindowsNetwork>();
    DWORD cookie = 0;
    if (FAILED(point->Advise(sink.Get(), &cookie))) {
        return;
    }

    NLM_CONNECTIVITY connectivity = NLM_CONNECTIVITY_DISCONNECTED;
    if (SUCCEEDED(manager->GetConnectivity(&connectivity))) {
        report(connectivity);
    }
}

HRESULT STDMETHODCALLTYPE WindowsNetwork::ConnectivityChanged(NLM_CONNECTIVITY connectivity) {
    report(connectivity);
    return S_OK;
}

void WindowsNetwork::report(NLM_CONNECTIVITY connectivity) {
    Player::postEvent({.type = Event::Type::NetworkChanged, .online = (connectivity & kConnected) != 0});
}

} // namespace haylen::platform
