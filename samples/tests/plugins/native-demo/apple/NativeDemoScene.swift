import CoreGraphics

// The picture of the demo, a small island with a palm under the sun in a sea with waves, in fractions of the width and the height of the context, the same on every platform. The video moves the waves and floats the sun with the time in seconds.
enum NativeDemoScene {
    private static let ink: UInt32 = 0x1E1B33
    private static let skyTop: UInt32 = 0x6CC6F5
    private static let skyLow: UInt32 = 0xCFF1FF
    private static let seaTop: UInt32 = 0x2FB3C4
    private static let seaLow: UInt32 = 0x1E8FA6
    private static let sun: UInt32 = 0xFFC93C
    private static let sunLight: UInt32 = 0xFFE89A
    private static let sand: UInt32 = 0xF3D58D
    private static let grass: UInt32 = 0x7BC74D
    private static let trunk: UInt32 = 0xA86A3D
    private static let leaf: UInt32 = 0x5DB33C
    private static let leafAngles: [CGFloat] = [-0.45, 0.4, 2.75, 3.55]

    // Draws the picture with its origin at the top left, so the first row of the bitmap is the top of the sky.
    static func draw(in context: CGContext, width: CGFloat, height: CGFloat, seconds: CGFloat) {
        context.saveGState()
        context.translateBy(x: 0, y: height)
        context.scaleBy(x: 1, y: -1)
        let horizon = 0.6 * height
        let outline = 0.012 * height
        fillGradient(context, CGRect(x: 0, y: 0, width: width, height: horizon), skyTop, skyLow)
        fillGradient(context, CGRect(x: 0, y: horizon, width: width, height: height - horizon), seaTop, seaLow)

        let sunY = 0.26 * height + 0.02 * height * sin(seconds * 1.2)
        context.setFillColor(color(sun))
        context.fillEllipse(in: circle(0.8 * width, sunY, 0.12 * height))
        context.setFillColor(color(sunLight))
        context.fillEllipse(in: circle(0.78 * width, sunY - 0.02 * height, 0.07 * height))

        let spacing = 0.2 * width
        context.setLineCap(.round)
        context.setLineWidth(0.014 * height)
        context.setStrokeColor(color(0xFFFFFF, alpha: 0.6))
        for row in 0..<3 {
            let waveY = horizon + (0.12 + 0.12 * CGFloat(row)) * height
            var start = (seconds * 0.06 * width * (1 + 0.5 * CGFloat(row)) + 0.07 * width * CGFloat(row)).truncatingRemainder(dividingBy: spacing) - spacing
            while start < width {
                context.move(to: CGPoint(x: start + 0.01 * width, y: waveY))
                context.addLine(to: CGPoint(x: start + 0.1 * width, y: waveY))
                start += spacing
            }
        }
        context.strokePath()

        outlinedEllipse(context, 0.36 * width, 0.64 * height, 0.22 * width, 0.085 * height, 0, outline, sand)
        outlinedEllipse(context, 0.36 * width, 0.6 * height, 0.15 * width, 0.05 * height, 0, outline, grass)
        let crown = CGPoint(x: 0.42 * width, y: 0.3 * height)
        for (lineWidth, trunkColor) in [(0.03 * height + outline * 2, ink), (0.03 * height, trunk)] {
            context.move(to: CGPoint(x: 0.38 * width, y: 0.6 * height))
            context.addLine(to: crown)
            context.setLineWidth(lineWidth)
            context.setStrokeColor(color(trunkColor))
            context.strokePath()
        }
        for angle in leafAngles {
            outlinedEllipse(context, crown.x + cos(angle) * 0.085 * width, crown.y + sin(angle) * 0.085 * width, 0.09 * width, 0.028 * height, angle, outline, leaf)
        }
        context.restoreGState()
    }

    private static func color(_ value: UInt32, alpha: CGFloat = 1) -> CGColor {
        CGColor(srgbRed: CGFloat((value >> 16) & 0xFF) / 255, green: CGFloat((value >> 8) & 0xFF) / 255, blue: CGFloat(value & 0xFF) / 255, alpha: alpha)
    }

    private static func circle(_ x: CGFloat, _ y: CGFloat, _ radius: CGFloat) -> CGRect {
        CGRect(x: x - radius, y: y - radius, width: radius * 2, height: radius * 2)
    }

    private static func fillGradient(_ context: CGContext, _ rect: CGRect, _ top: UInt32, _ bottom: UInt32) {
        let gradient = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB), colors: [color(top), color(bottom)] as CFArray, locations: [0, 1])!
        context.saveGState()
        context.clip(to: rect)
        context.drawLinearGradient(gradient, start: CGPoint(x: 0, y: rect.minY), end: CGPoint(x: 0, y: rect.maxY), options: [])
        context.restoreGState()
    }

    // Fills an ellipse over the outline of the art, a stroke twice as wide as the outline whose inner half the fill covers.
    private static func outlinedEllipse(_ context: CGContext, _ x: CGFloat, _ y: CGFloat, _ radiusX: CGFloat, _ radiusY: CGFloat, _ angle: CGFloat, _ outline: CGFloat, _ fill: UInt32) {
        var transform = CGAffineTransform(translationX: x, y: y).rotated(by: angle)
        let path = CGPath(ellipseIn: CGRect(x: -radiusX, y: -radiusY, width: radiusX * 2, height: radiusY * 2), transform: &transform)
        context.addPath(path)
        context.setLineWidth(outline * 2)
        context.setStrokeColor(color(ink))
        context.strokePath()
        context.addPath(path)
        context.setFillColor(color(fill))
        context.fillPath()
    }
}
