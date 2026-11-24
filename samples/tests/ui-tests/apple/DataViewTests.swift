import XCTest

// PLG-023: the Swift of the plugin "native-demo" fills camera frames in data views whose memory a texture of the app reads in place, and reads and marks a view that the app made, which the app then reads back from the same memory.
class DataViewTests: TestProjectCase {
    override var code: String {
        "PLG-023"
    }

    func testFramesArriveInDataViewsAndTheNativePartReadsAndWritesAViewOfTheApp() throws {
        expectCheck("Frames arrive in data views that update a texture in place", containing: "Swift filled 30 frames in views of 230400 bytes")
        expectCheck("The native part reads and writes a view of the app", containing: "Swift summed 4096 bytes to 505160, which Lua sums to 505160, and the view of Lua now starts with \"NATIVE\".")
        try saveScreenshot("data-views")
    }
}
