import Foundation

// Native part of the {{TITLE}} plugin on Apple platforms. The runtime creates it by the class name that plugin.json gives, and loads it while the app launches.
@objc({{NAME}}Plugin)
final class {{NAME}}Plugin: NSObject, HaylenPlugin {
    struct Echo: Codable {
        let message: String
    }

    func load(with context: HaylenPluginContext) {
        // Answers {{ID}}.echo with the message it receives, and sends it to the app again as the {{ID}}.echoed event.
        context.register("echo") { (params: Echo) async throws -> Echo in
            try context.emit("echoed", params)
            return params
        }
    }
}
