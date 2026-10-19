import Foundation
import ImageIO

#if !os(tvOS)
import AVFoundation
#endif

// The camera of the {{TITLE}} plugin on Apple platforms: AVFoundation into the video stream `camera`, and photos as JPEG bytes. Apple TV has no camera, so its calls fail there with the code `unsupported`.
final class {{NAME}}Camera {
    struct Facing: Decodable {
        let facing: String
    }

    struct Started: Encodable {
        let width: Int
        let height: Int
        let facing: String
    }

    struct Empty: Codable {}

    #if !os(tvOS)
    private var capture: {{NAME}}CameraCapture?
    #endif

    func register(_ context: HaylenPluginContext) {
        context.register("startCamera") { [unowned self] (params: Facing) async throws -> Started in
            #if os(tvOS)
            throw HaylenFailure("Apple TV has no camera.", code: "unsupported")
            #else
            try context.require(.usageDescription("NSCameraUsageDescription"))
            guard await AVCaptureDevice.requestAccess(for: .video) else {
                throw HaylenFailure("The person has not allowed the camera.", code: "permissionDenied")
            }
            guard let stream = context.openVideoStream("camera", width: {{NAME}}CameraCapture.width, height: {{NAME}}CameraCapture.height, format: .BGRA8) else {
                throw HaylenFailure("The video stream \"camera\" did not open.", code: "streamFailed")
            }
            self.capture?.stop()
            self.capture = try {{NAME}}CameraCapture(facing: params.facing, stream: stream)
            return Started(width: {{NAME}}CameraCapture.width, height: {{NAME}}CameraCapture.height, facing: params.facing)
            #endif
        }

        context.register("stopCamera") { [unowned self] (_: Empty) async throws -> Empty in
            #if !os(tvOS)
            self.capture?.stop()
            self.capture = nil
            #endif
            return Empty()
        }

        // Bytes travel next to the JSON, so the photo answers a dictionary with `Data` instead of a `Codable` value.
        context.registerHandler("takePhoto") { [unowned self] _, reply in
            #if os(tvOS)
            reply(false, ["message": "Apple TV has no camera.", "code": "unsupported"])
            #else
            guard let capture = self.capture else {
                reply(false, ["message": "The camera is off. Call \"startCamera\" first.", "code": "cameraOff"])
                return
            }
            guard let photo = capture.photo() else {
                reply(false, ["message": "The camera has no frame yet.", "code": "noFrame"])
                return
            }
            reply(true, ["jpeg": photo.jpeg, "width": photo.width, "height": photo.height])
            #endif
        }
    }
}

#if !os(tvOS)
// The capture of AVFoundation, whose BGRA frames of the size of the stream arrive on a dispatch queue of its own, which pushes them into the stream and keeps the newest one for a photo.
final class {{NAME}}CameraCapture: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate, @unchecked Sendable {
    static let width = 640
    static let height = 480

    private let session = AVCaptureSession()
    private let stream: HaylenVideoStream
    private let queue = DispatchQueue(label: "com.haylen.plugins.{{ID}}.camera")
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
#endif
