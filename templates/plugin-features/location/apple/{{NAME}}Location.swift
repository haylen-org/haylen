import CoreLocation
import Foundation

// The location of the {{TITLE}} plugin on Apple platforms: one fix of Core Location, or every change while the app watches, sent batched as the event `located`, once the app holds the permission `location` of the engine. Apple TV gives one fix but no updates, so watching fails there with the code `unsupported`.
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
