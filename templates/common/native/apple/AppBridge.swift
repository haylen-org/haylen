import Foundation

// The native bridge handlers of the app on macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS, which AppBridgeLoader.m registers when the app loads, before the runtime starts.
@objc final class AppBridge: NSObject {
    struct Greeting: Decodable {
        let name: String
    }

    struct Answer: Encodable {
        let message: String
    }

    struct Greeted: Encodable {
        let count: Int
    }

    @MainActor private static var greetings = 0

    // Answers "app.greet" with a greeting for the name that the app sends, and sends "app.greeted" with the count of greetings so far.
    @objc static func registerHandlers() {
        HaylenBridge.register("app.greet") { (greeting: Greeting) async throws -> Answer in
            greetings += 1
            try HaylenBridge.emit("app.greeted", Greeted(count: greetings))
            return Answer(message: "Hello, \(greeting.name), from Swift.")
        }
    }
}
