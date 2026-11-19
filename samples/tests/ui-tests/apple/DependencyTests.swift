import XCTest

// PLG-024: the Swift of the plugin "native-demo" calls the helper of the plugin "native-base" that it depends on, which the one app target compiles with it.
class DependencyTests: TestProjectCase {
    override var code: String {
        "PLG-024"
    }

    func testTheNativePartReachesThePluginItDependsOn() throws {
        expectCheck("The native part of \"native-demo\" reaches \"native-base\"", containing: "Swift called the helper of \"native-base\"")
        expectCheck("The native part of \"native-base\" answers the app", containing: "as the native part of native-base wrote it on")
        try saveScreenshot("dependency")
    }
}
