import Foundation

// Answers the methods of the platform handlers test with async Swift handlers: a greeting, a typed refusal, a thrown error and a slow call that reports its cancellation.
@objc final class NativeSampleHandlers: NSObject {
    struct Greeting: Decodable {
        let name: String
    }

    struct Answer: Encodable {
        let greeting: String
        let language: String
    }

    struct Nothing: Codable {}

    @objc static func registerHandlers() {
        HaylenBridge.register("native_sample.greet") { (params: Greeting) async throws -> Answer in
            try await Task.sleep(nanoseconds: 50_000_000)
            return Answer(greeting: "Hello, \(params.name)", language: "Swift")
        }
        HaylenBridge.register("native_sample.refuse") { (_: Nothing) async throws -> Nothing in
            throw HaylenFailure("Swift refused on purpose.", code: "refused", data: ["reason": "requested"])
        }
        HaylenBridge.register("native_sample.explode") { (_: Nothing) async throws -> Nothing in
            throw CocoaError(.featureUnsupported)
        }
        HaylenBridge.register("native_sample.slow") { (_: Nothing) async throws -> Nothing in
            do {
                try await Task.sleep(nanoseconds: 60_000_000_000)
            } catch {
                HaylenBridge.emit("native_sample.cancelled", payload: ["language": "Swift"])
                throw error
            }
            return Nothing()
        }
    }
}
