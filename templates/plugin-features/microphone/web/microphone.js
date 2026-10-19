// The microphone of the {{TITLE}} plugin on the web: `getUserMedia` into the audio stream `microphone`, through an audio worklet whose blocks of samples reach the page. The browser asks the person for the microphone, which it offers only to secure pages.
const failure = (message, code) => Object.assign(new Error(message), { code });

export default function registerMicrophone(context) {
    let recording = null;

    const stop = () => {
        recording?.media.getTracks().forEach((track) => track.stop());
        recording?.audio.close();
        recording = null;
    };

    context.register("startMicrophone", async () => {
        context.require({ secureContext: true, api: ["navigator.mediaDevices.getUserMedia", "AudioWorkletNode"], permissionsPolicy: "microphone" });
        stop();
        let media;
        try {
            media = await navigator.mediaDevices.getUserMedia({ audio: { channelCount: 1, echoCancellation: false } });
        } catch (error) {
            throw failure(`The microphone did not start: ${error.message}`, error.name === "NotAllowedError" ? "permissionDenied" : "unsupported");
        }
        const audio = new AudioContext();
        await audio.audioWorklet.addModule(new URL("./microphone-worklet.js", import.meta.url));
        const stream = context.audioStream("microphone", { sampleRate: audio.sampleRate, channels: 1 });
        const node = new AudioWorkletNode(audio, "{{ID}}-microphone");
        node.port.onmessage = (event) => stream.push(event.data);
        audio.createMediaStreamSource(media).connect(node);
        recording = { media, audio };
        return { sampleRate: audio.sampleRate, channels: 1 };
    });

    context.register("stopMicrophone", () => {
        stop();
        return null;
    });
}
