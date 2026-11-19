import CoreLocation
import Foundation
import UserNotifications

#if !os(tvOS)
import AVFoundation
#endif

// The capabilities of the demo that Apple platforms gate behind a prompt of the system, which the handlers ask for with the API of each capability right before they use it: the camera and the microphone of AVFoundation, the location of Core Location while the app is in use and the notifications of the notification center. The system shows each prompt while the person has not decided yet, and a refusal fails the call with the code `denied`. Apple TV has no camera, no microphone for apps and shows no notifications of the demo, so it asks only for the location.
final class NativeDemoAccess: NSObject, CLLocationManagerDelegate {
    private let context: HaylenPluginContext
    private let locations = CLLocationManager()
    private var deciding: [CheckedContinuation<Void, Never>] = []

    init(context: HaylenPluginContext) {
        self.context = context
        super.init()
        locations.delegate = self
    }

    #if !os(tvOS)
    // The system ends an app that asks for the camera or the microphone without the usage description of its `Info.plist`, so the plugin checks it first.
    @MainActor func requireCapture(_ media: AVMediaType) async throws {
        let usage = media == .video ? "NSCameraUsageDescription" : "NSMicrophoneUsageDescription"
        try context.require(.usageDescription(usage))
        if AVCaptureDevice.authorizationStatus(for: media) == .notDetermined {
            _ = await AVCaptureDevice.requestAccess(for: media)
        }
        guard AVCaptureDevice.authorizationStatus(for: media) == .authorized else {
            throw Self.refusal(media == .video ? "the camera" : "the microphone")
        }
    }

    // The notification center of a process without a bundle, such as a player run from a terminal, raises an exception, so the call fails with the code `unsupported` there.
    @MainActor func requireNotifications() async throws {
        guard Bundle.main.bundleIdentifier != nil else {
            throw HaylenFailure("The app runs without a bundle, so the system keeps no notifications for it.", code: "unsupported")
        }
        let center = UNUserNotificationCenter.current()
        if await center.notificationSettings().authorizationStatus == .notDetermined {
            _ = try await center.requestAuthorization(options: [.alert, .sound, .badge])
        }
        switch await center.notificationSettings().authorizationStatus {
        case .authorized, .provisional, .ephemeral:
            return
        default:
            throw Self.refusal("the notifications of the app")
        }
    }
    #endif

    // Core Location answers the request through a change of the authorization, which the delegate hears.
    @MainActor func requireLocation() async throws {
        #if os(macOS)
        try context.require(.usageDescription("NSLocationUsageDescription"))
        #else
        try context.require(.usageDescription("NSLocationWhenInUseUsageDescription"))
        #endif
        if locations.authorizationStatus == .notDetermined {
            await withCheckedContinuation { continuation in
                deciding.append(continuation)
                locations.requestWhenInUseAuthorization()
            }
        }
        switch locations.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse:
            return
        default:
            throw Self.refusal("the location")
        }
    }

    // Core Location also reports the authorization when the manager starts, which ends no request while the person has not decided.
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        Task { @MainActor in
            if self.locations.authorizationStatus == .notDetermined {
                return
            }
            let waiting = self.deciding
            self.deciding.removeAll()
            for continuation in waiting {
                continuation.resume()
            }
        }
    }

    private static func refusal(_ capability: String) -> HaylenFailure {
        HaylenFailure("The person has not allowed \(capability). The system asks only once, so the person allows it in the settings of the system.", code: "denied")
    }
}
