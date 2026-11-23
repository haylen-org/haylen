import XCTest

#if os(iOS)
// PLG-016: the back and the front cameras fill the video stream, a photo arrives as JPEG bytes that decode into a texture, and the microphone fills its stream with its level, after the prompts of the system. The simulator has no camera, and its microphone is the one of the Mac, which the Mac guards with a prompt of its own, so the test runs on a device.
class CameraTests: TestProjectCase {
    override var code: String {
        "PLG-016"
    }

    override var resetResources: [XCUIProtectedResource] {
        [.camera, .microphone]
    }

    func testTheCamerasThePhotoAndTheMicrophoneReachTheApp() throws {
#if targetEnvironment(simulator)
        throw XCTSkip("The simulator has no camera, and the Mac guards its microphone with a prompt that no test answers.")
#else
        tap("Start the back camera")
        answerAlert(.allow)
        expectCheck("The frames of the camera reach the texture", containing: "of the back camera")
        tap("Take a photo")
        expectCheck("A photo arrives as JPEG bytes", containing: "decoded into a texture")
        try saveScreenshot("back-camera")
        // Both camera buttons toggle, so the next one waits until the app shows the camera stopped, or a slow simulator would hand it the button while the camera still runs.
        tap("Stop the camera")
        waitFor(element("Start the back camera"))
        tap("Start the front camera")
        expectCheck("The frames of the camera reach the texture", containing: "of the front camera")
        try saveScreenshot("front-camera")
        tap("Stop the camera")
        waitFor(element("Start the back camera"))

        tap("Start the microphone")
        answerAlert(.allow)
        expectCheck("The microphone fills its stream and reports its level", containing: "level reports arrived")
        try saveScreenshot("microphone")
#endif
    }
}
#endif
