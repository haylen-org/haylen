import CoreLocation
import Foundation
import ImageIO
import MapKit

#if os(macOS)
import AppKit
#else
import UIKit
#endif

#if !os(tvOS)
import AVFoundation
#endif

// The device simulations of the demo on Apple platforms, which stand in for the SDKs of cameras, maps and sharing with the frameworks of the system alone: the camera into the video stream `camera` with photos as JPEG bytes, the microphone into the audio stream `microphone` with its level, the location of Core Location, a MapKit map over the app, the share sheet and other apps through their links. Apple TV has no camera, no microphone for apps and no share sheet, so those calls fail there with the code `unsupported`.
final class NativeDemoDevice: NSObject, CLLocationManagerDelegate {
    struct Empty: Codable {}

    struct Facing: Decodable {
        let facing: String
    }

    struct CameraStarted: Encodable {
        let width: Int
        let height: Int
        let facing: String
        let language: String
    }

    struct Photo: Encodable {
        let jpeg: Data
        let width: Int
        let height: Int
        let language: String
    }

    struct MicrophoneStarted: Encodable {
        let sampleRate: Int
        let channels: Int
        let language: String
    }

    struct Place: Encodable {
        let latitude: Double
        let longitude: Double
        let accuracy: Double
        let language: String
    }

    struct MapRequest: Decodable {
        let anchor: String
        let latitude: Double
        let longitude: Double
    }

    struct MapShown: Encodable {
        let anchor: String
        let latitude: Double
        let longitude: Double
        let drawnWith: String
        let language: String
    }

    struct Share: Decodable {
        let text: String
        let url: String?
    }

    struct Shared: Encodable {
        let shared: Bool?
        let language: String
    }

    struct OpenApp: Decodable {
        let kind: String
    }

    struct Opened: Encodable {
        let opened: Bool
        let language: String
    }

    private static let language = "Swift"

    private let context: HaylenPluginContext
    private let locations = CLLocationManager()
    private var located: [CheckedContinuation<CLLocation, Error>] = []
    private var map: HaylenOverlay.Item?
    #if !os(tvOS)
    private var camera: NativeDemoCamera?
    private var microphone: NativeDemoMicrophone?
    #endif

    init(context: HaylenPluginContext) {
        self.context = context
        super.init()
        locations.delegate = self
    }

    func register() {
        registerCamera()
        registerMicrophone()
        registerLocation()
        registerSharing()
    }

    // Ends what an earlier app of the process left running.
    func stop() {
        #if !os(tvOS)
        camera?.stop()
        camera = nil
        microphone?.stop()
        microphone = nil
        #endif
        map?.remove()
        map = nil
    }

    private func registerCamera() {
        context.register("startCamera") { [unowned self] (params: Facing) async throws -> CameraStarted in
            #if os(tvOS)
            throw HaylenFailure("Apple TV has no camera.", code: "unsupported")
            #else
            try self.context.require(.usageDescription("NSCameraUsageDescription"))
            guard await AVCaptureDevice.requestAccess(for: .video) else {
                throw HaylenFailure("The person has not allowed the camera.", code: "permissionDenied")
            }
            guard let stream = self.context.openVideoStream("camera", width: NativeDemoCamera.width, height: NativeDemoCamera.height, format: .BGRA8) else {
                throw HaylenFailure("The video stream \"camera\" did not open.", code: "streamFailed")
            }
            self.camera?.stop()
            self.camera = try NativeDemoCamera(facing: params.facing, stream: stream)
            return CameraStarted(width: NativeDemoCamera.width, height: NativeDemoCamera.height, facing: params.facing, language: Self.language)
            #endif
        }

        context.register("stopCamera") { [unowned self] (_: Empty) async throws -> Empty in
            #if !os(tvOS)
            self.camera?.stop()
            self.camera = nil
            #endif
            return Empty()
        }

        context.register("takePhoto") { [unowned self] (_: Empty) async throws -> Photo in
            #if os(tvOS)
            throw HaylenFailure("Apple TV has no camera.", code: "unsupported")
            #else
            guard let camera = self.camera else {
                throw HaylenFailure("The camera is off. Call \"startCamera\" first.", code: "cameraOff")
            }
            guard let photo = camera.photo() else {
                throw HaylenFailure("The camera has no frame yet.", code: "noFrame")
            }
            return Photo(jpeg: photo.jpeg, width: photo.width, height: photo.height, language: Self.language)
            #endif
        }
    }

    private func registerMicrophone() {
        context.register("startMicrophone") { [unowned self] (_: Empty) async throws -> MicrophoneStarted in
            #if os(tvOS)
            throw HaylenFailure("Apple TV gives apps no microphone.", code: "unsupported")
            #else
            try self.context.require(.usageDescription("NSMicrophoneUsageDescription"))
            guard await AVCaptureDevice.requestAccess(for: .audio) else {
                throw HaylenFailure("The person has not allowed the microphone.", code: "permissionDenied")
            }
            self.microphone?.stop()
            let context = self.context
            let microphone = try NativeDemoMicrophone(context: context) { level in
                context.emit("microphoneLevel", payload: ["level": level, "language": Self.language])
            }
            self.microphone = microphone
            return MicrophoneStarted(sampleRate: microphone.sampleRate, channels: 1, language: Self.language)
            #endif
        }

        context.register("stopMicrophone") { [unowned self] (_: Empty) async throws -> Empty in
            #if !os(tvOS)
            self.microphone?.stop()
            self.microphone = nil
            #endif
            return Empty()
        }
    }

    // The location asks for the permission while the app is in use, then for one fix of Core Location.
    private func registerLocation() {
        context.register("location") { [unowned self] (_: Empty) async throws -> Place in
            #if os(macOS)
            try self.context.require(.usageDescription("NSLocationUsageDescription"))
            #else
            try self.context.require(.usageDescription("NSLocationWhenInUseUsageDescription"))
            #endif
            let location = try await withCheckedThrowingContinuation { continuation in
                self.located.append(continuation)
                if self.locations.authorizationStatus == .notDetermined {
                    self.locations.requestWhenInUseAuthorization()
                } else {
                    self.locations.requestLocation()
                }
            }
            return Place(latitude: location.coordinate.latitude, longitude: location.coordinate.longitude, accuracy: location.horizontalAccuracy, language: Self.language)
        }

        context.register("showMap") { [unowned self] (params: MapRequest) async throws -> MapShown in
            guard let anchor = ["top": HaylenPlacement.Anchor.top, "bottom": .bottom][params.anchor] else {
                throw HaylenFailure("The anchor of the map is \"top\" or \"bottom\", not \"\(params.anchor)\".", code: "invalidAnchor")
            }
            let placement = HaylenPlacement(anchor: anchor)
            placement.width = 360
            placement.height = 220
            placement.margin = 16
            let view = MKMapView()
            let center = CLLocationCoordinate2D(latitude: params.latitude, longitude: params.longitude)
            view.setRegion(MKCoordinateRegion(center: center, latitudinalMeters: 2000, longitudinalMeters: 2000), animated: false)
            let pin = MKPointAnnotation()
            pin.coordinate = center
            pin.title = "Native Demo"
            view.addAnnotation(pin)
            self.map?.remove()
            self.map = self.context.overlay.add(view, placement: placement)
            return MapShown(anchor: params.anchor, latitude: params.latitude, longitude: params.longitude, drawnWith: "MapKit", language: Self.language)
        }

        context.register("removeMap") { [unowned self] (_: Empty) async throws -> Empty in
            self.map?.remove()
            self.map = nil
            return Empty()
        }
    }

    private func registerSharing() {
        context.register("share") { [unowned self] (params: Share) async throws -> Shared in
            let items: [Any] = [params.text] + (params.url.flatMap { URL(string: $0) }.map { [$0] } ?? [])
            #if os(tvOS)
            throw HaylenFailure("Apple TV has no share sheet.", code: "unsupported")
            #elseif os(macOS)
            guard let view = self.context.window?.contentView else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            self.context.coverApp()
            defer { self.context.uncoverApp() }
            return Shared(shared: await NativeDemoSharing.share(items, from: view), language: Self.language)
            #else
            guard let presenter = self.context.viewController else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            self.context.coverApp()
            defer { self.context.uncoverApp() }
            let shared: Bool = await withCheckedContinuation { continuation in
                let sheet = UIActivityViewController(activityItems: items, applicationActivities: nil)
                sheet.completionWithItemsHandler = { _, completed, _, _ in
                    continuation.resume(returning: completed)
                }
                // An iPad and Mac Catalyst show the sheet as a popover, which needs a place to point at.
                sheet.popoverPresentationController?.sourceView = presenter.view
                sheet.popoverPresentationController?.sourceRect = CGRect(x: presenter.view.bounds.midX, y: presenter.view.bounds.midY, width: 0, height: 0)
                presenter.present(sheet, animated: true)
            }
            return Shared(shared: shared, language: Self.language)
            #endif
        }

        // The settings of the app, the maps of the system at a place and a new message of the mail app open through their links.
        context.register("openApp") { (params: OpenApp) async throws -> Opened in
            #if os(macOS)
            let links = ["settings": "x-apple.systempreferences:", "maps": "maps://?ll=-22.9519,-43.2105", "mail": "mailto:demo@example.com?subject=Native%20Demo"]
            #else
            let links = ["settings": UIApplication.openSettingsURLString, "maps": "maps://?ll=-22.9519,-43.2105", "mail": "mailto:demo@example.com?subject=Native%20Demo"]
            #endif
            guard let link = links[params.kind], let url = URL(string: link) else {
                throw HaylenFailure("The app to open is \"settings\", \"maps\" or \"mail\", not \"\(params.kind)\".", code: "invalidApp")
            }
            #if os(macOS)
            return Opened(opened: NSWorkspace.shared.open(url), language: Self.language)
            #else
            return Opened(opened: await UIApplication.shared.open(url), language: Self.language)
            #endif
        }
    }

    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        let status = manager.authorizationStatus
        Task { @MainActor in
            self.authorizationChanged(status)
        }
    }

    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let location = locations.last else {
            return
        }
        Task { @MainActor in
            self.finishLocation(.success(location))
        }
    }

    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        let failure = HaylenFailure("The location is unknown: \(error.localizedDescription)", code: "locationUnknown")
        Task { @MainActor in
            self.finishLocation(.failure(failure))
        }
    }

    private func authorizationChanged(_ status: CLAuthorizationStatus) {
        guard !located.isEmpty else {
            return
        }
        switch status {
        case .notDetermined:
            return
        case .denied, .restricted:
            finishLocation(.failure(HaylenFailure("The person has not allowed the location.", code: "permissionDenied")))
        default:
            locations.requestLocation()
        }
    }

    private func finishLocation(_ result: Result<CLLocation, Error>) {
        let waiting = located
        located.removeAll()
        for continuation in waiting {
            continuation.resume(with: result)
        }
    }
}

#if !os(tvOS)
// The camera of the device through AVFoundation, whose frames arrive as BGRA pixel buffers of the size of the stream on a dispatch queue of its own, which pushes them into the stream and keeps the newest one for a photo.
final class NativeDemoCamera: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate, @unchecked Sendable {
    static let width = 640
    static let height = 480

    private let session = AVCaptureSession()
    private let stream: HaylenVideoStream
    private let queue = DispatchQueue(label: "dev.haylen.plugins.native-demo.camera")
    private let lock = NSLock()
    private var newest: CVPixelBuffer?

    init(facing: String, stream: HaylenVideoStream) throws {
        self.stream = stream
        super.init()
        #if os(macOS)
        let device = AVCaptureDevice.default(for: .video)
        #else
        let device = AVCaptureDevice.default(.builtInWideAngleCamera, for: .video, position: facing == "front" ? .front : .back) ?? AVCaptureDevice.default(for: .video)
        #endif
        guard let device, let input = try? AVCaptureDeviceInput(device: device), session.canAddInput(input) else {
            throw HaylenFailure("This device has no camera.", code: "unsupported")
        }
        session.beginConfiguration()
        session.sessionPreset = .vga640x480
        session.addInput(input)
        let output = AVCaptureVideoDataOutput()
        output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA, kCVPixelBufferWidthKey as String: Self.width, kCVPixelBufferHeightKey as String: Self.height]
        output.alwaysDiscardsLateVideoFrames = true
        output.setSampleBufferDelegate(self, queue: queue)
        guard session.canAddOutput(output) else {
            throw HaylenFailure("The camera cannot deliver frames to the app.", code: "unsupported")
        }
        session.addOutput(output)
        session.commitConfiguration()
        queue.async { [session] in
            session.startRunning()
        }
    }

    func stop() {
        queue.async { [session] in
            session.stopRunning()
        }
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        guard let buffer = CMSampleBufferGetImageBuffer(sampleBuffer) else {
            return
        }
        lock.lock()
        newest = buffer
        lock.unlock()
        stream.push(buffer, timestamp: CMSampleBufferGetPresentationTimeStamp(sampleBuffer).seconds)
    }

    // Encodes the newest frame as a JPEG file with ImageIO.
    func photo() -> (jpeg: Data, width: Int, height: Int)? {
        lock.lock()
        let buffer = newest
        lock.unlock()
        guard let buffer else {
            return nil
        }
        CVPixelBufferLockBaseAddress(buffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(buffer, .readOnly) }
        let width = CVPixelBufferGetWidth(buffer)
        let height = CVPixelBufferGetHeight(buffer)
        let bitmap = CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue
        guard let context = CGContext(data: CVPixelBufferGetBaseAddress(buffer), width: width, height: height, bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(buffer), space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: bitmap), let image = context.makeImage() else {
            return nil
        }
        let jpeg = NSMutableData()
        guard let destination = CGImageDestinationCreateWithData(jpeg as CFMutableData, "public.jpeg" as CFString, 1, nil) else {
            return nil
        }
        CGImageDestinationAddImage(destination, image, [kCGImageDestinationLossyCompressionQuality: 0.8] as CFDictionary)
        return CGImageDestinationFinalize(destination) ? (jpeg as Data, width, height) : nil
    }
}

// The microphone of the device through AVAudioEngine, whose input arrives as blocks of 32-bit floats on the thread of the audio system, which pushes the first channel into the stream and reports the level about ten times per second from that thread.
final class NativeDemoMicrophone: @unchecked Sendable {
    let sampleRate: Int

    private let engine = AVAudioEngine()
    private let stream: HaylenAudioStream
    private var measured = 0
    private var squares: Float = 0
    #if os(iOS)
    private let category: AVAudioSession.Category
    private let options: AVAudioSession.CategoryOptions
    #endif

    @MainActor
    init(context: HaylenPluginContext, level: @escaping (Double) -> Void) throws {
        #if os(iOS)
        // Recording needs a session that records, which mixes with the sound of the app, and the session of the app comes back when the microphone stops.
        let session = AVAudioSession.sharedInstance()
        category = session.category
        options = session.categoryOptions
        try session.setCategory(.playAndRecord, options: [.mixWithOthers, .defaultToSpeaker])
        try session.setActive(true)
        #endif
        let format = engine.inputNode.outputFormat(forBus: 0)
        guard format.sampleRate > 0, format.channelCount > 0 else {
            throw HaylenFailure("This device has no microphone.", code: "unsupported")
        }
        sampleRate = Int(format.sampleRate)
        guard let opened = context.openAudioStream("microphone", sampleRate: sampleRate, channels: 1, format: .float32, capacity: sampleRate) else {
            throw HaylenFailure("The audio stream \"microphone\" did not open.", code: "streamFailed")
        }
        stream = opened
        let blocksPerLevel = sampleRate / 10
        engine.inputNode.installTap(onBus: 0, bufferSize: 1024, format: format) { [unowned self] buffer, _ in
            guard let samples = buffer.floatChannelData?[0] else {
                return
            }
            let frames = Int(buffer.frameLength)
            self.stream.push(samples, frames: frames)
            for index in 0..<frames {
                self.squares += samples[index] * samples[index]
            }
            self.measured += frames
            if self.measured >= blocksPerLevel {
                level(Double((self.squares / Float(self.measured)).squareRoot()))
                self.measured = 0
                self.squares = 0
            }
        }
        try engine.start()
    }

    func stop() {
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        #if os(iOS)
        try? AVAudioSession.sharedInstance().setCategory(category, options: options)
        #endif
    }
}
#endif

#if os(macOS)
// The share picker of AppKit, shown next to the middle of the view of the app, which tells whether the person picked a service to share with.
@MainActor
final class NativeDemoSharing: NSObject, NSSharingServicePickerDelegate {
    private var finished: ((Bool) -> Void)?
    private var picker: NSSharingServicePicker?

    static func share(_ items: [Any], from view: NSView) async -> Bool {
        let sharing = NativeDemoSharing()
        return await withCheckedContinuation { continuation in
            sharing.finished = { continuation.resume(returning: $0) }
            let picker = NSSharingServicePicker(items: items)
            picker.delegate = sharing
            sharing.picker = picker
            picker.show(relativeTo: NSRect(x: view.bounds.midX, y: view.bounds.midY, width: 1, height: 1), of: view, preferredEdge: .minY)
        }
    }

    nonisolated func sharingServicePicker(_ sharingServicePicker: NSSharingServicePicker, didChoose service: NSSharingService?) {
        Task { @MainActor in
            self.finished?(service != nil)
            self.finished = nil
            self.picker = nil
        }
    }
}
#endif
