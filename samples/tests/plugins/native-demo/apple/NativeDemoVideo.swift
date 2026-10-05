import CoreGraphics
import CoreVideo
import Foundation

// The animated pattern of the video stream: stripes that move over a gradient whose colors turn, which CoreGraphics draws 30 times per second into pixel buffers on a dispatch queue of its own and pushes into the stream, the way a camera or a video decoder delivers its frames.
final class NativeDemoVideo {
    static let width = 320
    static let height = 180
    static let fps = 30

    private let stream: HaylenVideoStream
    private let timer = DispatchSource.makeTimerSource(queue: DispatchQueue(label: "dev.haylen.plugins.native-demo.video"))
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
            paint(context)
        }
        CVPixelBufferUnlockBaseAddress(buffer, [])
        stream.push(buffer, timestamp: Double(frame) / Double(Self.fps))
        frame += 1
    }

    private func paint(_ context: CGContext) {
        let width = CGFloat(Self.width)
        let height = CGFloat(Self.height)
        let turn = CGFloat(frame % 180) / 180
        let colors = [CGColor(srgbRed: turn, green: 0.3, blue: 1 - turn, alpha: 1), CGColor(srgbRed: 1 - turn, green: 0.8, blue: turn, alpha: 1)] as CFArray
        let gradient = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB), colors: colors, locations: [0, 1])!
        context.drawLinearGradient(gradient, start: .zero, end: CGPoint(x: width, y: height), options: [])

        context.setFillColor(CGColor(srgbRed: 1, green: 1, blue: 1, alpha: 0.35))
        var offset = -height + CGFloat(frame % 32) * 2
        while offset < width {
            context.move(to: CGPoint(x: offset, y: 0))
            context.addLine(to: CGPoint(x: offset + 16, y: 0))
            context.addLine(to: CGPoint(x: offset + 16 + height, y: height))
            context.addLine(to: CGPoint(x: offset + height, y: height))
            context.closePath()
            offset += 64
        }
        context.fillPath()
    }
}
