import XCTest

#if os(iOS)
// INP-015: the virtual controller of GameController drives a gamepad of the app through the same reading of controllers as the Apple TV. Its stick moves the focus of a compass up, down, left and right, a push up with a little sideways lean moves it up, and its south button presses the focused button. The controller connects again in the gamepad it had, and a gamepad that the simulator connects of its own stays still while the virtual one acts. iOS lets an app connect one virtual controller at a time, so the virtual controllers of SDL in the engine tests check several gamepads at once, and tvOS has no virtual controller, so the remote and the engine tests of the focus cover it there.
class StickFocusTests: TestProjectCase {
    // How far a push leans the stick, past the press threshold of the focus, and how long it holds there.
    static let lean: CGFloat = 0.8
    static let hold: TimeInterval = 0.5

    override var code: String {
        "INP-015"
    }

    func testTheStickMovesTheFocusInEveryDirection() throws {
        waitFor(element(startingWith: "The virtual controller \""))
        let pushes: [(String, CGVector)] = [
            ("North", CGVector(dx: 0, dy: -1)),
            ("South", CGVector(dx: 0, dy: 1)),
            ("West", CGVector(dx: -1, dy: 0)),
            ("East", CGVector(dx: 1, dy: 0)),
            ("North", CGVector(dx: 0.3, dy: -1)),
        ]
        for (index, (name, direction)) in pushes.enumerated() {
            waitFor(element("The focus rests on Middle."))
            push(direction)
            waitFor(element("Move \(index + 1) took the focus from Middle to \(name)."), timeout: 10)
        }
        try saveScreenshot("stick-focus")

        waitFor(element("The focus rests on Middle."))
        waitFor(element("virtual-south")).press(forDuration: Self.hold)
        waitFor(element("The south button pressed Middle."), timeout: 10)
    }

    // The engine keeps the gamepad of a controller while it stays connected and gives it the first free one when it connects, so the virtual controller that leaves and connects again takes the gamepad it had, beside a gamepad of the simulator that stays where it was.
    func testTheVirtualControllerConnectsAgainInTheGamepadItHad() throws {
        let first = waitFor(element(matching: "Connection 1 of the virtual controller took gamepad \\d+\\.")).label
        let gamepad = try XCTUnwrap(first.split(separator: " ").last?.dropLast(), "The line \"\(first)\" names no gamepad.")
        let none = element("No other gamepad is connected.")
        let other = element(matching: "Gamepad \\d+ \".*\" stays still\\.")
        waitUntil("The test tells nothing about the other gamepads.") { none.exists || other.exists }
        let others = none.exists ? "" : other.label
        tap("Connect the controller again")
        waitFor(element("Connection 2 of the virtual controller took gamepad \(gamepad)."))
        if !others.isEmpty {
            waitFor(element(others))
        }

        waitFor(element("The focus rests on Middle."))
        push(CGVector(dx: 0, dy: -1))
        waitFor(element("Move 1 took the focus from Middle to North."), timeout: 10)
    }

    // A gamepad that the simulator connects of its own, which no test drives, stays still while the stick and the south button of the virtual controller act, so each gamepad keeps the input of its own player.
    func testAnotherGamepadStaysStillWhileTheVirtualOneActs() throws {
        waitFor(element(startingWith: "Connection 1 of the virtual controller"))
        let none = element("No other gamepad is connected.")
        let other = element(matching: "Gamepad \\d+ \".*\" stays still\\.")
        waitUntil("The test tells nothing about the other gamepads.") { none.exists || other.exists }
        if none.exists {
            throw XCTSkip("This simulator connects no gamepad of its own, so no second gamepad can stay still beside the virtual controller, whose single connection per app iOS allows.")
        }

        let still = other.label
        push(CGVector(dx: 0, dy: -1))
        waitFor(element("Move 1 took the focus from Middle to North."), timeout: 10)
        waitFor(element("The focus rests on Middle."))
        waitFor(element("virtual-south")).press(forDuration: Self.hold)
        waitFor(element("The south button pressed Middle."), timeout: 10)
        XCTAssertTrue(element(still).exists, "The other gamepad acted while the virtual controller did: \(element(startingWith: "Gamepad ").label)")
    }

    // Leans the stick from its middle in the direction, holds it and lets it go, as a thumb does.
    func push(_ direction: CGVector) {
        let stick = waitFor(element("virtual-stick"))
        let reach = stick.frame.width / 2 * Self.lean
        let length = (direction.dx * direction.dx + direction.dy * direction.dy).squareRoot()
        let middle = stick.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
        let leaned = middle.withOffset(CGVector(dx: direction.dx / length * reach, dy: direction.dy / length * reach))
        middle.press(forDuration: 0.05, thenDragTo: leaned, withVelocity: .fast, thenHoldForDuration: Self.hold)
    }
}
#endif
