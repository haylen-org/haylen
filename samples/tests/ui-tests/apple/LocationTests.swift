import CoreLocation
import XCTest

#if !os(visionOS)
// PLG-017: after the prompt of the system, the plugin tells where the device is, which the test simulates on the simulator and on the device alike, and shows a native map over the app at that place.
class LocationTests: TestProjectCase {
    static let place = CLLocation(latitude: 48.85837, longitude: 2.29448)

    override var code: String {
        "PLG-017"
    }

    override var resetResources: [XCUIProtectedResource] {
        [.location]
    }

    func testThePlaceAndTheMapReachTheApp() throws {
        XCUIDevice.shared.location = XCUILocation(location: Self.place)
        activate("Find the location")
        answerAlert(["Allow While Using App"])
        expectCheck("The platform tells where the device is", containing: String(format: "found %.5f, %.5f", Self.place.coordinate.latitude, Self.place.coordinate.longitude))
        activate("Show the map", after: [.down])
        expectCheck("A native map shows over the app", containing: "placed a map drawn with")
        try saveScreenshot("map")
    }
}
#endif
