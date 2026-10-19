// Hands every block of the first channel of the microphone to the page, which pushes it into the audio stream of the {{TITLE}} plugin.
class Microphone extends AudioWorkletProcessor {
    process(inputs) {
        const channel = inputs[0][0];
        if (channel) {
            this.port.postMessage(channel.slice());
        }
        return true;
    }
}

registerProcessor("{{ID}}-microphone", Microphone);
