import Foundation

// A failure that a Swift handler throws to fail its call with a code that tells failures apart and data for the app.
struct HaylenFailure: Error {
    let message: String
    let code: String?
    let data: (any Encodable & Sendable)?

    init(_ message: String, code: String? = nil, data: (any Encodable & Sendable)? = nil) {
        self.message = message
        self.code = code
        self.data = data
    }
}

extension HaylenBridge {
    // Registers a handler written as an async function on the main actor, whose parameters decode from the JSON of the call and whose result encodes to the JSON of the answer. A thrown HaylenFailure fails the call with its code and data, any other error fails it with the code exception, and the task is cancelled when the app cancels the call or its timeout passes.
    static func register<Params: Decodable, Result: Encodable>(_ method: String, handler: @escaping @MainActor (Params) async throws -> Result) {
        registerCancellableHandler(method) { params, reply in
            let task = Task { @MainActor in
                do {
                    let input = try JSONSerialization.data(withJSONObject: params, options: .fragmentsAllowed)
                    let result = try await handler(JSONDecoder().decode(Params.self, from: input))
                    reply(true, try JSONSerialization.jsonObject(with: JSONEncoder().encode(result), options: .fragmentsAllowed))
                } catch is CancellationError {
                    return
                } catch let failure as HaylenFailure {
                    var answer: [String: Any] = ["message": failure.message]
                    answer["code"] = failure.code
                    if let data = failure.data {
                        answer["data"] = try? JSONSerialization.jsonObject(with: JSONEncoder().encode(data), options: .fragmentsAllowed)
                    }
                    reply(false, answer)
                } catch {
                    reply(false, ["message": String(describing: error), "code": "exception", "data": ["type": String(reflecting: type(of: error))]])
                }
            }
            return { task.cancel() }
        }
    }
}
