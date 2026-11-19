import Foundation

// The helper that the plugin `native-base` offers to the plugins that depend on it. The Apple sources of every plugin of an app compile into its one app target, so their Swift calls it directly.
enum NativeBase {
    static func describe(_ text: String) -> String {
        "\(text), as the native part of native-base wrote it on \(platform)."
    }

    private static var platform: String {
        #if os(macOS)
        return "macOS"
        #elseif targetEnvironment(macCatalyst)
        return "Mac Catalyst"
        #elseif os(tvOS)
        return "tvOS"
        #elseif os(visionOS)
        return "visionOS"
        #else
        return "iOS"
        #endif
    }
}
