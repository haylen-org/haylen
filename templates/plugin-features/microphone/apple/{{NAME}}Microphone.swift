import Foundation

#if !os(tvOS)
import AVFoundation
#endif

// The microphone of the {{TITLE}} plugin on Apple platforms: AVAudioEngine into the audio stream `microphone`, after the person allowed the microphone. Apple TV gives apps no microphone, so its calls fail there with the code `unsupported`.
final class {{NAME}}Microphone {
    struct Started: Encodable {
        let sampleRate: Int
        let channels: Int
    }

    struct Empty: Codable {}

    #if !os(tvOS)
    private var recording: {{NAME}}MicrophoneRecording?
    #endif

    func register(_ context: HaylenPluginContext) {
        context.register("startMicrophone") { [unowned self] (_: Empty) async throws -> Started in
            #if os(tvOS)
            throw HaylenFailure("Apple TV gives apps no microphone.", code: "unsupported")
            #else
            try context.require(.usageDescription("NSMicrophoneUsageDescription"))
            guard await AVCaptureDevice.requestAccess(for: .audio) else {
                throw HaylenFailure("The person has not allowed the microphone.", code: "permissionDenied")
            }
            self.recording?.stop()
            let recording = try {{NAME}}MicrophoneRecording(context: context)
            self.recording = recording
            return Started(sampleRate: recording.sampleRate, channels: 1)
            #endif
        }

        context.register("stopMicrophone") { [unowned self] (_: Empty) async throws -> Empty in
            #if !os(tvOS)
            self.recording?.stop()
            self.recording = nil
            #endif
            return Empty()
        }
    }
}

#if !os(tvOS)
// The recording of AVAudioEngine, whose input arrives as blocks of 32-bit floats on the thread of the audio system, which pushes the first channel into the stream.
final class {{NAME}}MicrophoneRecording: @unchecked Sendable {
    let sampleRate: Int

    private let engine = AVAudioEngine()
    private let stream: HaylenAudioStream
    #if os(iOS)
    private let category: AVAudioSession.Category
    private let options: AVAudioSession.CategoryOptions
    #endif

    @MainActor
    init(context: HaylenPluginContext) throws {
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
        engine.inputNode.installTap(onBus: 0, bufferSize: 1024, format: format) { [stream] buffer, _ in
            if let samples = buffer.floatChannelData?[0] {
                stream.push(samples, frames: Int(buffer.frameLength))
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
