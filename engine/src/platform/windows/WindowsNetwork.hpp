#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <netlistmgr.h>
#include <wrl/client.h>
#include <wrl/implements.h>

namespace haylen::platform {

// Follows whether the device reaches a network through the network list manager of Windows, whose connectivity changes reach the window thread, which runs the messages of the window. The device is online while any network gives it IPv4 or IPv6 connectivity, local or to the internet.
class WindowsNetwork final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, INetworkListManagerEvents> {
  public:
    // Reports the connectivity now and subscribes to its changes for the life of the process, on the thread of the window.
    static void observe();

    HRESULT STDMETHODCALLTYPE ConnectivityChanged(NLM_CONNECTIVITY connectivity) override;

  private:
    static constexpr int kConnected = NLM_CONNECTIVITY_IPV4_SUBNET | NLM_CONNECTIVITY_IPV4_LOCALNETWORK | NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_SUBNET | NLM_CONNECTIVITY_IPV6_LOCALNETWORK | NLM_CONNECTIVITY_IPV6_INTERNET;

    static void report(NLM_CONNECTIVITY connectivity);

    // The manager and its connection point live as long as the process, which keeps the subscription.
    static Microsoft::WRL::ComPtr<INetworkListManager>& manager;
    static Microsoft::WRL::ComPtr<IConnectionPoint>& point;
};

} // namespace haylen::platform
