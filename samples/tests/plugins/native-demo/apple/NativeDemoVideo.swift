import CoreGraphics
import CoreVideo
import Foundation

// The picture of the demo in the video stream, with its waves moving and its sun floating, which CoreGraphics draws 30 times per second into pixel buffers on a dispatch queue of its own and pushes into the stream, the way a camera or a video decoder delivers its frames.
final class NativeDemoVideo {
    static let width = 320
    static let height = 180
    static let fps = 30

    private let stream: HaylenVideoStream
    private let timer = DispatchSource.makeTimerSource(queue: DispatchQueue(label: "com.haylen.plugins.native-demo.video"))
    private var pool: CVPixelBufferPool?
    private var frame = 0

    init(stream: HaylenVideoStream) {
        self.stream = stream
        let attributes = [kCVPixelBufferPixelFormatTypeKey: kCVPixelFormatType_32BGRA, kCVPixelBufferWidthKey: Self.width, kCVPixelBufferHeightKey: Self.height] as CFDictionary
        CVPixelBufferPoolCreate(nil, nil, attributes, &pool)
        timer.schedule(deadline: .now(), repeating: 1.0 / Double(Self.fps))
        timer.setEventHandler { [unowned self] in
            self.draw()
        }
        timer.resume()
    }

    func stop() {
        timer.cancel()
    }

    // BGRA pixels in memory are 32-bit words in little endian order with alpha first, which is how CoreGraphics draws into the pixel buffer.
    private func draw() {
        var buffer: CVPixelBuffer?
        guard let pool, CVPixelBufferPoolCreatePixelBuffer(nil, pool, &buffer) == kCVReturnSuccess, let buffer else {
            return
        }
        CVPixelBufferLockBaseAddress(buffer, [])
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let bitmap = CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue
        if let context = CGContext(data: CVPixelBufferGetBaseAddress(buffer), width: Self.width, height: Self.height, bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(buffer), space: space, bitmapInfo: bitmap) {
            NativeDemoScene.draw(in: context, width: CGFloat(Self.width), height: CGFloat(Self.height), seconds: CGFloat(frame) / CGFloat(Self.fps))
        }
        CVPixelBufferUnlockBaseAddress(buffer, [])
        stream.push(buffer, timestamp: Double(frame) / Double(Self.fps))
        frame += 1
    }
}
