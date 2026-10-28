import XCTest

#if os(iOS)
// The framebuffer of the app follows its window through the sizes of iPadOS: a window that fills the screen, tiled to each half and resized there, and resized by its corner in Stage Manager. The test "Window and device" shows the framebuffer, and the Debug engine checks at every frame that the view and its drawable cover the window, logging an error that fails the run when they part.
class WindowSizeTests: HaylenUITestCase {
    var multitasking: Multitasking?

    override func setUpWithError() throws {
        try super.setUpWithError()
        try XCTSkipUnless(isPad, "Only an iPad resizes the windows of apps.")
        multitasking = setMultitasking(.windowedApps)
        tap("Platform")
        tap("PLT-003  Window and device")
        expectFramebufferFollowsWindow("full")
    }

    override func tearDown() {
        if let multitasking {
            setMultitasking(.windowedApps)
            arrangeWindow("Fill")
            setMultitasking(multitasking)
        }
        super.tearDown()
    }

    func testTheFramebufferFollowsTheWindowInSplitView() throws {
        // A window that fills the screen hides its controls, so a drag of its corner makes it a window of its own first.
        resizeWindow(by: CGVector(dx: -300, dy: -300))
        expectFramebufferFollowsWindow("windowed")
        arrangeWindow("Left")
        expectFramebufferFollowsWindow("left-half")
        resizeWindow(by: CGVector(dx: -160, dy: 0))
        expectFramebufferFollowsWindow("left-narrow")
        resizeWindow(by: CGVector(dx: 320, dy: 0))
        expectFramebufferFollowsWindow("left-wide")
        arrangeWindow("Right")
        expectFramebufferFollowsWindow("right-half")
    }

    func testTheFramebufferFollowsTheWindowInStageManager() throws {
        setMultitasking(.stageManager)
        expectFramebufferFollowsWindow("stage-manager")
        for (index, distance) in [CGVector(dx: -300, dy: -400), CGVector(dx: 120, dy: 250), CGVector(dx: -200, dy: 60)].enumerated() {
            resizeWindow(by: distance)
            expectFramebufferFollowsWindow("stage-manager-\(index + 1)")
        }
    }

    // Waits until the app reports a framebuffer of the pixels of its window, its frame in points times its scale cut to whole pixels, and saves a screenshot of the window.
    func expectFramebufferFollowsWindow(_ name: String, file: StaticString = #filePath, line: UInt = #line) {
        let readout = element(matching: "\\d+ by \\d+ pixels at a scale of [0-9.]+")
        waitFor(readout, file: file, line: line)
        var reported = ""
        var expected = ""
        waitUntil("At the size \"\(name)\" the app reports \(reported), while its window needs \(expected).", file: file, line: line) {
            reported = readout.label
            let numbers = reported.split(separator: " ").compactMap { Double($0) }
            guard numbers.count == 3 else { return false }
            let frame = windowFrame
            expected = "\(Int(frame.width * numbers[2])) by \(Int(frame.height * numbers[2])) pixels"
            return Int(numbers[0]) == Int(frame.width * numbers[2]) && Int(numbers[1]) == Int(frame.height * numbers[2])
        }
        let controls = Self.springboard.descendants(matching: .any)["window-controls:\(identifier!)"].firstMatch
        if controls.exists {
            XCTAssertFalse(element("Back").frame.intersects(controls.frame), "At the size \"\(name)\" the controls of the window cover the button \"Back\" of the app.", file: file, line: line)
        }
        try? saveScreenshot("window-\(name)")
    }
}
#endif
