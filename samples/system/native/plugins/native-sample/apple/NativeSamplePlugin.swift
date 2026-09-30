import Foundation

// Native part of the native sample on Apple platforms, with async Swift handlers: a greeting, a typed refusal, a thrown error and a slow call that reports its cancellation.
@objc(NativeSamplePlugin)
final class NativeSamplePlugin: NSObject, HaylenPlugin {
    struct Greeting: Decodable {
        let name: String
    }

    struct Answer: Encodable {
        let greeting: String
        let language: String
    }

    struct Nothing: Codable {}

    func load(with context: HaylenPluginContext) {
        context.register("greet") { (params: Greeting) async throws -> Answer in
            try await Task.sleep(nanoseconds: 50_000_000)
            return Answer(greeting: "Hello, \(params.name)", language: "Swift")
        }
        context.register("refuse") { (_: Nothing) async throws -> Nothing in
            throw HaylenFailure("Swift refused on purpose.", code: "refused", data: ["reason": "requested"])
        }
        context.register("explode") { (_: Nothing) async throws -> Nothing in
            throw CocoaError(.featureUnsupported)
        }
        context.register("slow") { (_: Nothing) async throws -> Nothing in
            do {
                try await Task.sleep(nanoseconds: 60_000_000_000)
            } catch {
                context.emit("cancelled", payload: ["language": "Swift"])
                throw error
            }
            return Nothing()
        }
    }
}
