import Foundation

// Native part of the plugin `native-base` on Apple platforms, whose method answers the app with the text of its helper `NativeBase`.
@objc(NativeBasePlugin)
final class NativeBasePlugin: NSObject, HaylenPlugin {
    struct Describe: Decodable {
        let text: String
    }

    struct Described: Encodable {
        let text: String
        let language: String
    }

    func load(with context: HaylenPluginContext) {
        context.register("describe") { (params: Describe) async throws -> Described in
            Described(text: NativeBase.describe(params.text), language: "Swift")
        }
    }
}
