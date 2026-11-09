// The microphone of the {{TITLE}} plugin on the web: `getUserMedia` into the audio stream `microphone`, through an audio worklet whose blocks of samples reach the page, once the page holds the permission `microphone`, which the browser offers only to secure pages.
const failure = (message, code) => Object.assign(new Error(message), { code });

// The page asks for the microphone by opening it, and closes it at once.
const requestMicrophone = async () => {
    try {
        const media = await navigator.mediaDevices.getUserMedia({ audio: true });
        media.getTracks().forEach((track) => track.stop());
        return "granted";
    } catch (error) {
        if (error.name === "NotAllowedError") {
            return "denied";
        }
        if (error.name === "NotFoundError" || error.name === "OverconstrainedError") {
            return { state: "unsupported", message: "The device has no microphone." };
        }
        return "unknown";
    }
};

export default function registerMicrophone(context) {
    context.registerPermission({
        name: "microphone",
        capability: "the microphone",
        needs: { secureContext: true, api: "navigator.mediaDevices.getUserMedia", permissionsPolicy: "microphone" },
        query: "microphone",
        request: requestMicrophone,
    });

    let recording = null;

    const stop = () => {
        recording?.media.getTracks().forEach((track) => track.stop());
        recording?.audio.close();
        recording = null;
    };

    context.register("startMicrophone", async () => {
        context.require({ api: "AudioWorkletNode" });
        await context.requirePermission("microphone");
        stop();
        let media;
        try {
            media = await navigator.mediaDevices.getUserMedia({ audio: { channelCount: 1, echoCancellation: false } });
        } catch (error) {
            throw failure(`The microphone did not start: ${error.message}`, "microphoneFailed");
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
