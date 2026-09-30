import Foundation
import ImageIO

#if os(macOS)
import AppKit
#else
import UIKit
#endif

// Native part of the Native Demo plugin on iOS, iPadOS, Mac Catalyst, tvOS and macOS, built on Foundation, UIKit and AppKit alone. The runtime creates it by the class name that plugin.json gives and loads it while the app launches.
@objc(NativeDemoPlugin)
final class NativeDemoPlugin: NSObject, HaylenPlugin {
    struct Empty: Codable {}

    struct Compute: Decodable {
        let limit: Int
    }

    struct Computed: Encodable {
        let primes: Int
        let thread: String
        let detail: String
        let language: String
    }

    struct Failure: Encodable {
        let reason: String
        let language: String
    }

    struct Wait: Decodable {
        let token: Int
    }

    struct Ticks: Decodable {
        let enabled: Bool
        let interval: Double
    }

    struct Ticking: Encodable {
        let enabled: Bool
        let interval: Double
    }

    struct Banner: Decodable {
        let anchor: String
        let reserve: Bool
    }

    struct Visibility: Decodable {
        let visible: Bool
    }

    struct BannerState: Encodable {
        let anchor: String
        let reserve: Bool
        let visible: Bool
    }

    struct Screen: Decodable {
        let title: String?
    }

    struct Closed: Encodable {
        let seconds: Double
    }

    struct Picked: Encodable {
        let name: String
    }

    struct Burst: Decodable {
        let count: Int
        let ticks: Int
    }

    struct Bursting: Encodable {
        let count: Int
        let ticks: Int
        let language: String
    }

    private nonisolated static let language = "Swift"

    private var context: HaylenPluginContext!
    private var ticker: Timer?
    private var ticks = 0
    private var bursts: Timer?
    private var burst = (count: 0, ticks: 0, tick: 0)
    private var banner: NativeDemoBanner?
    private var bannerState = BannerState(anchor: "bottom", reserve: false, visible: false)
    private var bannerTaps = 0
    private var lastError: [String: Any]?

    func load(with context: HaylenPluginContext) {
        self.context = context
        registerCalls(context)
        registerBytes(context)
        registerEvents(context)
        registerStreams(context)
        registerBanner(context)
        registerScreens(context)
        context.emitRetained("loaded", payload: ["language": Self.language, "platform": Self.platform])
    }

    private func registerCalls(_ context: HaylenPluginContext) {
        // A handler of the Objective-C API answers any JSON value on the main queue.
        context.registerHandler("echo") { params, reply in
            let value = (params as? [String: Any])?["value"] ?? NSNull()
            reply(true, ["echo": value, "thread": Thread.isMainThread ? "main" : "background", "language": Self.language])
        }

        context.register("compute") { (params: Compute) async throws -> Computed in
            await withCheckedContinuation { continuation in
                DispatchQueue.global(qos: .userInitiated).async {
                    let primes = Self.countPrimes(below: params.limit)
                    let queue = String(cString: __dispatch_queue_get_label(nil))
                    continuation.resume(returning: Computed(primes: primes, thread: Thread.isMainThread ? "main" : "background", detail: "the dispatch queue " + queue, language: Self.language))
                }
            }
        }

        context.register("fail") { (_: Empty) async throws -> Empty in
            throw HaylenFailure("The native demo failed on purpose.", code: "demoFailure", data: Failure(reason: "requested", language: Self.language))
        }

        // The call never answers by itself, so only a cancel or a timeout of the app ends the sleep, and the plugin tells the app that it heard it.
        context.register("wait") { (params: Wait) async throws -> Empty in
            do {
                try await Task.sleep(nanoseconds: UInt64.max)
            } catch {
                context.emit("waitCancelled", payload: ["token": params.token, "language": Self.language])
                throw error
            }
            return Empty()
        }

        context.registerHandler("config") { _, reply in
            reply(true, context.config)
        }

        // Every app that loads the Lua API sends start. The plugin ends what an earlier app of the process left running and hands the new app the error that stopped the earlier one.
        context.register("start") { [unowned self] (_: Empty) async throws -> Empty in
            self.stopTicking()
            self.stopBursts()
            self.banner?.remove()
            self.banner = nil
            if let failure = self.lastError {
                context.emitRetained("lastError", payload: failure)
                self.lastError = nil
            }
            return Empty()
        }
    }

    // Bytes cross as Data in the dictionaries of the Objective-C API of the context, since the helpers for Codable values carry JSON alone.
    private func registerBytes(_ context: HaylenPluginContext) {
        context.registerHandler("echoBytes") { params, reply in
            guard let data = (params as? [String: Any])?["data"] as? Data else {
                reply(false, ["message": "echoBytes needs bytes.", "code": "invalidParams"])
                return
            }
            reply(true, ["data": data, "size": data.count, "thread": Thread.isMainThread ? "main" : "background", "language": Self.language])
        }

        context.registerHandler("generatedImage") { params, reply in
            let values = params as? [String: Any]
            let width = values?["width"] as? Int ?? 0
            let height = values?["height"] as? Int ?? 0
            guard (1...2048).contains(width), (1...2048).contains(height), let png = Self.drawPattern(width: width, height: height) else {
                reply(false, ["message": "generatedImage needs a width and a height from 1 to 2048.", "code": "invalidParams"])
                return
            }
            reply(true, ["png": png, "width": width, "height": height, "drawnWith": "CoreGraphics and ImageIO", "language": Self.language])
        }
    }

    // Video and audio streams from Swift need the stream API of the Apple runtime, which a later version of the engine brings, so the plugin says so instead.
    private func registerStreams(_ context: HaylenPluginContext) {
        for method in ["startVideo", "stopVideo", "startTone", "stopTone"] {
            context.register(method) { (_: Empty) async throws -> Empty in
                throw HaylenFailure("The Apple runtime of this engine has no stream API yet, so Swift cannot push video frames or audio samples to the app.", code: "unsupported")
            }
        }
    }

    private func registerEvents(_ context: HaylenPluginContext) {
        context.register("burst") { [unowned self] (params: Burst) async throws -> Bursting in
            self.startBursts(count: params.count, ticks: params.ticks)
            return Bursting(count: params.count, ticks: params.ticks, language: Self.language)
        }

        context.register("ticks") { [unowned self] (params: Ticks) async throws -> Ticking in
            self.stopTicking()
            if params.enabled {
                self.ticks = 0
                let timer = Timer(timeInterval: params.interval, target: self, selector: #selector(self.tick), userInfo: nil, repeats: true)
                RunLoop.main.add(timer, forMode: .common)
                self.ticker = timer
            }
            return Ticking(enabled: params.enabled, interval: params.interval)
        }
    }

    @objc private func tick() {
        ticks += 1
        context.emit("tick", payload: ["count": ticks, "thread": Thread.isMainThread ? "main" : "background", "language": Self.language])
    }

    private func stopTicking() {
        ticker?.invalidate()
        ticker = nil
    }

    // Sends count batched events 30 times per second for ticks ticks, which reach the app as one list per frame, and then burstDone.
    private func startBursts(count: Int, ticks: Int) {
        stopBursts()
        burst = (count: count, ticks: ticks, tick: 0)
        let timer = Timer(timeInterval: 1.0 / 30.0, target: self, selector: #selector(sendBurst), userInfo: nil, repeats: true)
        RunLoop.main.add(timer, forMode: .common)
        bursts = timer
    }

    @objc private func sendBurst() {
        for index in 0..<burst.count {
            context.emit("burst", payload: ["tick": burst.tick, "index": index, "language": Self.language], retain: false, batched: true)
        }
        burst.tick += 1
        if burst.tick == burst.ticks {
            stopBursts()
            context.emit("burstDone", payload: ["events": burst.count * burst.ticks, "ticks": burst.ticks, "language": Self.language])
        }
    }

    private func stopBursts() {
        bursts?.invalidate()
        bursts = nil
    }

    private func registerBanner(_ context: HaylenPluginContext) {
        context.register("showBanner") { [unowned self] (params: Banner) async throws -> BannerState in
            guard let anchor = ["top": HaylenPlacement.Anchor.top, "bottom": .bottom][params.anchor] else {
                throw HaylenFailure("The anchor of the banner is top or bottom, not \(params.anchor).", code: "invalidAnchor")
            }
            let placement = HaylenPlacement(anchor: anchor)
            placement.reserve = params.reserve
            placement.width = NativeDemoBanner.width
            placement.height = NativeDemoBanner.height
            if let banner = self.banner {
                banner.place(placement)
            } else {
                let text = context.config["greeting"] as? String ?? ""
                self.banner = NativeDemoBanner(text: text, color: try Self.color(of: context), overlay: context.overlay, placement: placement) { [unowned self] in
                    self.bannerTaps += 1
                    context.emit("bannerTapped", payload: ["count": self.bannerTaps, "language": Self.language])
                }
            }
            self.bannerState = BannerState(anchor: params.anchor, reserve: params.reserve, visible: self.banner?.isVisible ?? false)
            return self.bannerState
        }

        context.register("setBannerVisible") { [unowned self] (params: Visibility) async throws -> BannerState in
            guard let banner = self.banner else {
                throw HaylenFailure("No banner shows. Call showBanner first.", code: "noBanner")
            }
            banner.isVisible = params.visible
            self.bannerState = BannerState(anchor: self.bannerState.anchor, reserve: self.bannerState.reserve, visible: params.visible)
            return self.bannerState
        }

        context.register("removeBanner") { [unowned self] (_: Empty) async throws -> Empty in
            self.banner?.remove()
            self.banner = nil
            return Empty()
        }
    }

    // The native screen and the file picker cover the app while they show, so the app stands still and stays silent until they close.
    private func registerScreens(_ context: HaylenPluginContext) {
        context.register("showScreen") { (params: Screen) async throws -> Closed in
            let started = Date()
            let color = try Self.color(of: context)
            #if os(macOS)
            guard let window = context.window else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            context.coverApp()
            defer { context.uncoverApp() }
            await NativeDemoScreen.show(title: params.title ?? "Native screen", color: color, over: window)
            #else
            guard let presenter = Self.topmost(context.viewController) else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            context.coverApp()
            defer { context.uncoverApp() }
            await NativeDemoScreen.show(title: params.title ?? "Native screen", color: color, over: presenter)
            #endif
            return Closed(seconds: Date().timeIntervalSince(started))
        }

        context.register("pickFile") { (_: Empty) async throws -> Picked? in
            #if os(tvOS)
            throw HaylenFailure("tvOS has no file picker.", code: "unsupported")
            #elseif os(macOS)
            guard let window = context.window else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            context.coverApp()
            defer { context.uncoverApp() }
            return await NativeDemoPicker.pick(over: window).map { Picked(name: $0) }
            #else
            guard let presenter = Self.topmost(context.viewController) else {
                throw HaylenFailure("The app has no window yet.", code: "noWindow")
            }
            context.coverApp()
            defer { context.uncoverApp() }
            return await NativeDemoPicker.pick(over: presenter).map { Picked(name: $0) }
            #endif
        }
    }

    // The error screen of the app shows this error. The plugin keeps it and hands it to the next app when that app sends start.
    func appDidFail(with error: [String: Any]) {
        lastError = ["message": error["message"] ?? "", "file": error["file"] ?? "", "line": error["line"] ?? 0, "language": Self.language]
    }

    #if os(macOS)
    func application(_ application: NSApplication, open urls: [URL]) {
        for url in urls {
            context.emitRetained("urlOpened", payload: ["url": url.absoluteString])
        }
    }
    #else
    // The runtime hands the links that launch the app here too, right after the scene connects, and the retained event waits for the first listener.
    func scene(_ scene: UIScene, openURLContexts URLContexts: Set<UIOpenURLContext>) {
        for opened in URLContexts {
            context.emitRetained("urlOpened", payload: ["url": opened.url.absoluteString])
        }
    }

    private static func topmost(_ root: UIViewController?) -> UIViewController? {
        var top = root
        while let presented = top?.presentedViewController {
            top = presented
        }
        return top
    }
    #endif

    private static var platform: String {
        #if os(macOS)
        return "macos"
        #elseif targetEnvironment(macCatalyst)
        return "catalyst"
        #elseif os(tvOS)
        return "tvos"
        #else
        return "ios"
        #endif
    }

    // The bannerColor parameter as a color, which is #RRGGBB.
    private static func color(of context: HaylenPluginContext) throws -> CGColor {
        let text = context.config["bannerColor"] as? String ?? ""
        guard text.count == 7, text.hasPrefix("#"), let value = UInt32(text.dropFirst(), radix: 16) else {
            throw HaylenFailure("The bannerColor parameter must be a color as #RRGGBB, not \(text).", code: "invalidColor")
        }
        return CGColor(srgbRed: CGFloat((value >> 16) & 0xFF) / 255, green: CGFloat((value >> 8) & 0xFF) / 255, blue: CGFloat(value & 0xFF) / 255, alpha: 1)
    }

    // Draws the pattern of the demo, a gradient from red to green with blue stripes, with CoreGraphics and encodes it as a PNG file with ImageIO.
    private static func drawPattern(width: Int, height: Int) -> Data? {
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        guard let context = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8, bytesPerRow: 0, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
            return nil
        }
        let colors = [CGColor(srgbRed: 1, green: 0, blue: 0, alpha: 1), CGColor(srgbRed: 0, green: 1, blue: 0, alpha: 1)] as CFArray
        let gradient = CGGradient(colorsSpace: space, colors: colors, locations: [0, 1])!
        context.drawLinearGradient(gradient, start: CGPoint(x: 0, y: height), end: CGPoint(x: width, y: 0), options: [])
        context.setFillColor(CGColor(srgbRed: 0, green: 0, blue: 0.9, alpha: 0.6))
        var offset = -CGFloat(height)
        while offset < CGFloat(width) {
            context.move(to: CGPoint(x: offset, y: CGFloat(height)))
            context.addLine(to: CGPoint(x: offset + 16, y: CGFloat(height)))
            context.addLine(to: CGPoint(x: offset + 16 + CGFloat(height), y: 0))
            context.addLine(to: CGPoint(x: offset + CGFloat(height), y: 0))
            context.closePath()
            offset += 32
        }
        context.fillPath()
        guard let image = context.makeImage() else {
            return nil
        }
        let png = NSMutableData()
        guard let destination = CGImageDestinationCreateWithData(png as CFMutableData, "public.png" as CFString, 1, nil) else {
            return nil
        }
        CGImageDestinationAddImage(destination, image, nil)
        return CGImageDestinationFinalize(destination) ? png as Data : nil
    }

    private nonisolated static func countPrimes(below limit: Int) -> Int {
        guard limit > 2 else {
            return 0
        }
        var composite = [Bool](repeating: false, count: limit)
        var count = 0
        for number in 2..<limit where !composite[number] {
            count += 1
            var multiple = number * number
            while multiple < limit {
                composite[multiple] = true
                multiple += number
            }
        }
        return count
    }
}
