// AudioWorklet processor of the audio output of the Haylen web runtime, which `haylen-runtime.js` loads from next to its script.
// The page mixes on its own thread and posts blocks of interleaved samples, which the processor keeps in a ring buffer and plays one render quantum at a time. It answers with the frames it played since its last answer and the buffers of the blocks it copied, which the page fills again, and it counts the render quanta that found the buffer empty.

class HaylenAudioProcessor extends AudioWorkletProcessor {
    constructor(options) {
        super();
        const { channels, capacity, reportFrames } = options.processorOptions;
        this.channels = channels;
        this.capacity = capacity;
        this.reportFrames = reportFrames;
        this.ring = new Float32Array(capacity * channels);
        this.readFrame = 0;
        this.storedFrames = 0;
        this.unreportedFrames = 0;
        this.underruns = 0;
        this.spent = [];
        // An inactive output plays silence and keeps what it has queued. Only an output that received samples since it became active counts underruns.
        this.active = false;
        this.primed = false;
        this.port.onmessage = (event) => this.receive(event.data);
    }

    receive(data) {
        if (data instanceof Float32Array) {
            this.write(data);
            return;
        }
        this.active = data.active;
        this.primed = false;
    }

    write(block) {
        const frames = block.length / this.channels;
        const writeFrame = (this.readFrame + this.storedFrames) % this.capacity;
        const firstFrames = Math.min(frames, this.capacity - writeFrame);
        this.ring.set(block.subarray(0, firstFrames * this.channels), writeFrame * this.channels);
        this.ring.set(block.subarray(firstFrames * this.channels), 0);
        this.storedFrames += frames;
        this.primed = true;
        this.spent.push(block.buffer);
    }

    process(inputs, outputs) {
        const output = outputs[0];
        const frames = output[0].length;
        const played = this.active ? Math.min(frames, this.storedFrames) : 0;
        for (let channel = 0; channel < output.length; ++channel) {
            const samples = output[channel];
            let frame = this.readFrame;
            for (let index = 0; index < played; ++index) {
                samples[index] = this.ring[frame * this.channels + channel];
                frame = frame + 1 === this.capacity ? 0 : frame + 1;
            }
            samples.fill(0, played);
        }
        this.readFrame = (this.readFrame + played) % this.capacity;
        this.storedFrames -= played;
        this.unreportedFrames += played;

        // A starved output answers at once, so the page learns about it without waiting for a whole block to play.
        const starved = this.active && this.primed && played < frames;
        if (starved) {
            this.underruns += 1;
        }
        if (this.unreportedFrames >= this.reportFrames || (starved && this.unreportedFrames > 0)) {
            this.port.postMessage({ played: this.unreportedFrames, underruns: this.underruns, buffers: this.spent }, this.spent);
            this.unreportedFrames = 0;
            this.spent = [];
        }
        return true;
    }
}

registerProcessor("haylen-audio", HaylenAudioProcessor);
