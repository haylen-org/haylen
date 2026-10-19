import CoreLocation
import Foundation

// The location of the {{TITLE}} plugin on Apple platforms: one fix of Core Location, or every change while the app watches, sent batched as the event `located`, after the person allowed the location while the app is in use. Apple TV gives one fix but no updates, so watching fails there with the code `unsupported`.
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
    private var watching = false

    override init() {
        super.init()
        manager.delegate = self
    }

    func register(_ context: HaylenPluginContext) {
        self.context = context
        context.register("locate") { [unowned self] (_: Empty) async throws -> Place in
            try self.requireUsage()
            let location = try await withCheckedThrowingContinuation { continuation in
                self.waiting.append(continuation)
                self.askOrLocate()
            }
            return Self.place(of: location)
        }

        context.register("watchLocation") { [unowned self] (_: Empty) async throws -> Empty in
            #if os(tvOS)
            throw HaylenFailure("Apple TV gives one fix of the location but no updates.", code: "unsupported")
            #else
            try self.requireUsage()
            self.watching = true
            self.askOrLocate()
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

    private func requireUsage() throws {
        #if os(macOS)
        try context?.require(.usageDescription("NSLocationUsageDescription"))
        #else
        try context?.require(.usageDescription("NSLocationWhenInUseUsageDescription"))
        #endif
    }

    // Asks for the permission the first time, and starts what waits once the person answered.
    private func askOrLocate() {
        switch manager.authorizationStatus {
        case .notDetermined:
            manager.requestWhenInUseAuthorization()
        case .denied, .restricted:
            fail(HaylenFailure("The person has not allowed the location.", code: "permissionDenied"))
        default:
            start()
        }
    }

    private func start() {
        if !waiting.isEmpty {
            manager.requestLocation()
        }
        #if !os(tvOS)
        if watching {
            manager.startUpdatingLocation()
        }
        #endif
    }

    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        Task { @MainActor in
            guard !self.waiting.isEmpty || self.watching, manager.authorizationStatus != .notDetermined else {
                return
            }
            self.askOrLocate()
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
