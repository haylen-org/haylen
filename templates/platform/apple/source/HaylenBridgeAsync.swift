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
        registerCancellableHandler(method, handler: cancellable(handler))
    }

    // Sends an event whose payload encodes to JSON, retained for the first listener of its name when retain is true. Throws the error of the encoder when the payload does not encode.
    static func emit<Payload: Encodable>(_ event: String, _ payload: Payload, retain: Bool = false) throws {
        emit(event, payload: try jsonObject(payload), retain: retain)
    }

    fileprivate static func cancellable<Params: Decodable, Result: Encodable>(_ handler: @escaping @MainActor (Params) async throws -> Result) -> HaylenCancellableHandler {
        return { params, reply in
            let task = Task { @MainActor in
                do {
                    let input = try JSONSerialization.data(withJSONObject: params, options: .fragmentsAllowed)
                    let result = try await handler(JSONDecoder().decode(Params.self, from: input))
                    reply(true, try jsonObject(result))
                } catch is CancellationError {
                    return
                } catch let failure as HaylenFailure {
                    var answer: [String: Any] = ["message": failure.message]
                    answer["code"] = failure.code
                    if let data = failure.data {
                        answer["data"] = try? jsonObject(data)
                    }
                    reply(false, answer)
                } catch {
                    reply(false, ["message": String(describing: error), "code": "exception", "data": ["type": String(reflecting: type(of: error))]])
                }
            }
            return { task.cancel() }
        }
    }

    fileprivate static func jsonObject(_ value: some Encodable) throws -> Any {
        try JSONSerialization.jsonObject(with: JSONEncoder().encode(value), options: .fragmentsAllowed)
    }
}

extension HaylenPluginContext {
    // Registers the handler of <id>.<method> written as an async function, like HaylenBridge.register.
    func register<Params: Decodable, Result: Encodable>(_ method: String, handler: @escaping @MainActor (Params) async throws -> Result) {
        registerCancellableHandler(method, handler: HaylenBridge.cancellable(handler))
    }

    // Sends the event <id>.<event> with a payload that encodes to JSON, retained for the first listener of its name when retain is true. Throws the error of the encoder when the payload does not encode.
    func emit<Payload: Encodable>(_ event: String, _ payload: Payload, retain: Bool = false) throws {
        let object = try HaylenBridge.jsonObject(payload)
        if retain {
            emitRetained(event, payload: object)
        } else {
            emit(event, payload: object)
        }
    }
}
