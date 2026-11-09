import CoreLocation
import Foundation
import UserNotifications

#if !os(tvOS)
import AVFoundation
#endif

// The permissions that the demo owns on Apple platforms, which the runtime answers for `system.permission` and `system.requestPermission`: the camera and the microphone of AVFoundation, the location of Core Location while the app is in use and the notifications of the notification center. Apple TV has no camera, no microphone for apps and shows no notifications of the demo, so it registers only the location.
final class NativeDemoPermissions: NSObject, CLLocationManagerDelegate {
    private let locations = CLLocationManager()
    private var asking: [HaylenPermissionAnswer] = []

    override init() {
        super.init()
        locations.delegate = self
    }

    func register(_ context: HaylenPluginContext) {
        #if !os(tvOS)
        context.register(Self.capture("camera", capability: "the camera", usage: "NSCameraUsageDescription", media: .video))
        context.register(Self.capture("microphone", capability: "the microphone", usage: "NSMicrophoneUsageDescription", media: .audio))
        context.register(Self.notifications())
        #endif
        context.register(location())
    }

    #if !os(tvOS)
    private static func capture(_ name: String, capability: String, usage: String, media: AVMediaType) -> HaylenPermission {
        HaylenPermission(name: name, capability: capability, requirements: [.usageDescription(usage)], state: { answer in
            answer(state(of: AVCaptureDevice.authorizationStatus(for: media)), nil)
        }, request: { answer in
            AVCaptureDevice.requestAccess(for: media) { granted in
                answer(granted ? .granted : .denied, nil)
            }
        })
    }

    private static func state(of status: AVAuthorizationStatus) -> HaylenPermission.State {
        switch status {
        case .notDetermined:
            return .prompt
        case .authorized:
            return .granted
        case .denied, .restricted:
            return .denied
        @unknown default:
            return .unknown
        }
    }

    // The notification center of a process without a bundle, such as a player run from a terminal, raises an exception, so it answers unsupported instead.
    private static func notifications() -> HaylenPermission {
        HaylenPermission(name: "notifications", capability: "notifications", requirements: [], state: { answer in
            guard Bundle.main.bundleIdentifier != nil else {
                answer(.unsupported, "The app runs without a bundle, so the system keeps no notifications for it.")
                return
            }
            UNUserNotificationCenter.current().getNotificationSettings { settings in
                answer(state(of: settings.authorizationStatus), nil)
            }
        }, request: { answer in
            UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge]) { granted, _ in
                answer(granted ? .granted : .denied, nil)
            }
        })
    }

    private static func state(of status: UNAuthorizationStatus) -> HaylenPermission.State {
        switch status {
        case .notDetermined:
            return .prompt
        case .denied:
            return .denied
        case .authorized, .provisional, .ephemeral:
            return .granted
        @unknown default:
            return .unknown
        }
    }
    #endif

    // Core Location answers a request through a change of the authorization.
    private func location() -> HaylenPermission {
        #if os(macOS)
        let usage = "NSLocationUsageDescription"
        #else
        let usage = "NSLocationWhenInUseUsageDescription"
        #endif
        return HaylenPermission(name: "location", capability: "the location", requirements: [.usageDescription(usage)], state: { [unowned self] answer in
            answer(Self.state(of: self.locations.authorizationStatus), nil)
        }, request: { [unowned self] answer in
            self.asking.append(answer)
            self.locations.requestWhenInUseAuthorization()
        })
    }

    private static func state(of status: CLAuthorizationStatus) -> HaylenPermission.State {
        switch status {
        case .notDetermined:
            return .prompt
        case .authorizedAlways, .authorizedWhenInUse:
            return .granted
        case .denied, .restricted:
            return .denied
        @unknown default:
            return .unknown
        }
    }

    // Core Location also reports the authorization when the manager starts, which answers no request while the person has not decided.
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        Task { @MainActor in
            let state = Self.state(of: self.locations.authorizationStatus)
            if state == .prompt {
                return
            }
            let answers = self.asking
            self.asking.removeAll()
            for answer in answers {
                answer(state, nil)
            }
        }
    }
}
