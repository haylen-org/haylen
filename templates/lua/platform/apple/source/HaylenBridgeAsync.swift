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

    // Sends an event whose payload encodes to JSON, retained for the first listener of its name when retain is true, and batched with the events of its name in the same frame when batched is true. Throws the error of the encoder when the payload does not encode.
    static func emit<Payload: Encodable>(_ event: String, _ payload: Payload, retain: Bool = false, batched: Bool = false) throws {
        emit(event, payload: try jsonObject(payload), retain: retain, batched: batched)
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

    // Registers the screen `<id>.<name>` with a handler on the main actor, whose parameters decode from the JSON of the app with `JSONDecoder`, and which shows its UI through the screen. A thrown `HaylenFailure` fails the screen with its code and data, and any other error fails it with the code `exception`.
    func registerScreen<Params: Decodable>(_ name: String, handler: @escaping @MainActor (Params, HaylenScreen) throws -> Void) {
        registerScreen(name) { params, screen in
            Task { @MainActor in
                do {
                    let input = try JSONSerialization.data(withJSONObject: params, options: .fragmentsAllowed)
                    try handler(JSONDecoder().decode(Params.self, from: input), screen)
                } catch let failure as HaylenFailure {
                    screen.fail(failure.message, code: failure.code, data: failure.data.flatMap { try? HaylenBridge.jsonObject($0) })
                } catch {
                    screen.fail(String(describing: error), code: "exception", data: ["type": String(reflecting: type(of: error))])
                }
            }
        }
    }

    // Sends the event <id>.<event> with a payload that encodes to JSON, retained for the first listener of its name when retain is true, and batched with the events of its name in the same frame when batched is true. Throws the error of the encoder when the payload does not encode.
    func emit<Payload: Encodable>(_ event: String, _ payload: Payload, retain: Bool = false, batched: Bool = false) throws {
        emit(event, payload: try HaylenBridge.jsonObject(payload), retain: retain, batched: batched)
    }

    // Checks that the project of the app holds what the plugin needs, such as `.usageDescription("NSCameraUsageDescription")`, before the plugin calls the system API that needs it. Throws a `HaylenFailure` with the code `unsupported`, whose data lists each missing requirement in `missing` as `{kind, name, file, snippet}`, after it logged each missing one once.
    func require(_ requirements: HaylenRequirement...) throws {
        do {
            try self.requirements.require(requirements)
        } catch {
            let failure = (error as NSError).userInfo
            throw HaylenFailure(failure["message"] as? String ?? error.localizedDescription, code: "unsupported", data: failure["data"] as? [String: [[String: String]]])
        }
    }
}

extension HaylenScreen {
    // Ends the screen with a result that encodes to JSON with `JSONEncoder`. Throws the error of the encoder when the result does not encode.
    func finish<Result: Encodable>(encoding result: Result) throws {
        finish(try HaylenBridge.jsonObject(result))
    }
}
