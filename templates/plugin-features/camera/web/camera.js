// The camera of the {{TITLE}} plugin on the web: `getUserMedia` into the video stream `camera`, and photos as JPEG bytes, once the page holds the permission `camera`, which the browser offers only to secure pages.
const failure = (message, code) => Object.assign(new Error(message), { code });

// The page asks for the camera by opening it, and closes it at once.
const requestCamera = async () => {
    try {
        const media = await navigator.mediaDevices.getUserMedia({ video: true });
        media.getTracks().forEach((track) => track.stop());
        return "granted";
    } catch (error) {
        if (error.name === "NotAllowedError") {
            return "denied";
        }
        if (error.name === "NotFoundError" || error.name === "OverconstrainedError") {
            return { state: "unsupported", message: "The device has no camera." };
        }
        return "unknown";
    }
};

export default function registerCamera(context) {
    context.registerPermission({
        name: "camera",
        capability: "the camera",
        needs: { secureContext: true, api: "navigator.mediaDevices.getUserMedia", permissionsPolicy: "camera" },
        query: "camera",
        request: requestCamera,
    });

    const stream = context.videoStream("camera");
    let media = null;
    let video = null;
    let frame = 0;

    const stop = () => {
        cancelAnimationFrame(frame);
        media?.getTracks().forEach((track) => track.stop());
        media = null;
        video = null;
    };

    // Pushes the frame that the video shows on every frame of the page.
    const push = () => {
        if (video && video.readyState >= video.HAVE_CURRENT_DATA) {
            stream.push(video);
        }
        frame = requestAnimationFrame(push);
    };

    context.register("startCamera", async (params) => {
        await context.requirePermission("camera");
        const facing = params.facing || "back";
        stop();
        try {
            media = await navigator.mediaDevices.getUserMedia({ video: { width: 640, height: 480, facingMode: facing === "front" ? "user" : "environment" } });
        } catch (error) {
            throw failure(`The camera did not start: ${error.message}`, "cameraFailed");
        }
        video = document.createElement("video");
        video.muted = true;
        video.playsInline = true;
        video.srcObject = media;
        await video.play();
        push();
        return { width: video.videoWidth, height: video.videoHeight, facing };
    });

    context.register("stopCamera", () => {
        stop();
        return null;
    });

    context.register("takePhoto", async () => {
        if (!video) {
            throw failure('The camera is off. Call "startCamera" first.', "cameraOff");
        }
        const canvas = document.createElement("canvas");
        canvas.width = video.videoWidth;
        canvas.height = video.videoHeight;
        canvas.getContext("2d").drawImage(video, 0, 0);
        const blob = await new Promise((resolve) => canvas.toBlob(resolve, "image/jpeg", 0.8));
        return { jpeg: new Uint8Array(await blob.arrayBuffer()), width: canvas.width, height: canvas.height };
    });
}
