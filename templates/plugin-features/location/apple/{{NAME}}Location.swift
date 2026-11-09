import CoreLocation
import Foundation

// The location of the {{TITLE}} plugin on Apple platforms: one fix of Core Location, or every change while the app watches, sent batched as the event `located`, once the app holds the permission `location` that the plugin owns, the location while the app is in use. Apple TV gives one fix but no updates, so watching fails there with the code `unsupported`.
final class {{NAME}}Location: NSObject, CLLocationManagerDelegate {
    struct Place: Encodable {
        let latitude: Double
        let longitude: Double
        let accuracy: Double
    }

    struct Empty: Codable {}

    private var context: HaylenPluginContext?
    private let manager = CLLocationManager()
    private var waiting: [CheckedContinuation<CLLocation, Error>] = []
    private var asking: [HaylenPermissionAnswer] = []
    private var watching = false

    override init() {
        super.init()
        manager.delegate = self
    }

    func register(_ context: HaylenPluginContext) {
        self.context = context
        context.register(permission())

        context.register("locate") { [unowned self] (_: Empty) async throws -> Place in
            try await context.requirePermission("location")
            let location = try await withCheckedThrowingContinuation { continuation in
                self.waiting.append(continuation)
                self.manager.requestLocation()
            }
            return Self.place(of: location)
        }

        context.register("watchLocation") { [unowned self] (_: Empty) async throws -> Empty in
            #if os(tvOS)
            throw HaylenFailure("Apple TV gives one fix of the location but no updates.", code: "unsupported")
            #else
            try await context.requirePermission("location")
            self.watching = true
            self.manager.startUpdatingLocation()
            return Empty()
            #endif
        }

        context.register("stopWatchingLocation") { [unowned self] (_: Empty) async throws -> Empty in
            self.watching = false
            #if !os(tvOS)
            self.manager.stopUpdatingLocation()
            #endif
            return Empty()
        }
    }

    // The permission `location` that the plugin owns, which Core Location asks for once the `Info.plist` gives the text of its prompt, and whose answer arrives as a change of the authorization.
    private func permission() -> HaylenPermission {
        let usage = HaylenRequirement.usageDescription(Self.usageKey)
        return HaylenPermission(name: "location", capability: "the location", requirements: [usage], state: { [unowned self] answer in
            answer(Self.state(of: self.manager.authorizationStatus), nil)
        }, request: { [unowned self] answer in
            self.asking.append(answer)
            self.manager.requestWhenInUseAuthorization()
        })
    }

    #if os(macOS)
    private static let usageKey = "NSLocationUsageDescription"
    #else
    private static let usageKey = "NSLocationWhenInUseUsageDescription"
    #endif

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
            let state = Self.state(of: self.manager.authorizationStatus)
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

    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        Task { @MainActor in
            guard let location = locations.last else {
                return
            }
            self.finish(.success(location))
            if self.watching {
                for location in locations {
                    try? self.context?.emit("located", Self.place(of: location), batched: true)
                }
            }
        }
    }

    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        Task { @MainActor in
            self.fail(HaylenFailure("The location is unknown: \(error.localizedDescription)", code: "locationUnknown"))
        }
    }

    private func fail(_ failure: HaylenFailure) {
        watching = false
        finish(.failure(failure))
    }

    private func finish(_ result: Result<CLLocation, Error>) {
        let continuations = waiting
        waiting.removeAll()
        for continuation in continuations {
            continuation.resume(with: result)
        }
    }

    private static func place(of location: CLLocation) -> Place {
        Place(latitude: location.coordinate.latitude, longitude: location.coordinate.longitude, accuracy: location.horizontalAccuracy)
    }
}
