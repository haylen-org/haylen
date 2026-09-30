import Foundation

// A sine wave that a dispatch queue of its own synthesizes a tenth of a second ahead of the clock into the audio stream, the way a synthesized voice or decoded network audio arrives, as mono 32-bit floats at 44100 Hz.
final class NativeDemoTone {
    static let sampleRate = 44100

    private let stream: HaylenAudioStream
    private let step: Double
    private let timer = DispatchSource.makeTimerSource(queue: DispatchQueue(label: "dev.haylen.plugins.native-demo.tone"))
    private let started = DispatchTime.now()
    private var written = 0
    private var phase = 0.0
    private var block = [Float](repeating: 0, count: NativeDemoTone.sampleRate / 100)

    init(frequency: Double, stream: HaylenAudioStream) {
        self.stream = stream
        step = 2 * Double.pi * frequency / Double(Self.sampleRate)
        timer.schedule(deadline: .now(), repeating: .milliseconds(10))
        timer.setEventHandler { [unowned self] in
            self.fill()
        }
        timer.resume()
    }

    func stop() {
        timer.cancel()
    }

    private func fill() {
        let seconds = Double(DispatchTime.now().uptimeNanoseconds - started.uptimeNanoseconds) / 1_000_000_000
        let target = Int((seconds + 0.1) * Double(Self.sampleRate))
        while written < target {
            for index in block.indices {
                block[index] = Float(sin(phase)) * 0.3
                phase = (phase + step).truncatingRemainder(dividingBy: 2 * Double.pi)
            }
            stream.push(block, frames: block.count)
            written += block.count
        }
    }
}
