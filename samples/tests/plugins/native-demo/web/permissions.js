// The permissions that the demo owns on the web, which the runtime answers for `system.permission` and `system.requestPermission`: the camera and the microphone of `getUserMedia`, the location of `navigator.geolocation` and the notifications of the page.
const kLocationRequestMilliseconds = 30000;

// The page asks for the camera or the microphone by opening it, and closes it at once.
const requestMedia = async (constraints, absent) => {
    try {
        const media = await navigator.mediaDevices.getUserMedia(constraints);
        media.getTracks().forEach((track) => track.stop());
        return "granted";
    } catch (error) {
        if (error.name === "NotAllowedError") {
            return "denied";
        }
        if (error.name === "NotFoundError" || error.name === "OverconstrainedError") {
            return { state: "unsupported", message: absent };
        }
        return "unknown";
    }
};

// A fix that may come from the cache asks the person, and a fix that does not come in time leaves the state unknown, since the person may still answer.
const requestLocation = () => new Promise((resolve) => {
    navigator.geolocation.getCurrentPosition(() => resolve("granted"), (error) => resolve(error.code === error.PERMISSION_DENIED ? "denied" : "unknown"), { maximumAge: Infinity, timeout: kLocationRequestMilliseconds });
});

const notificationState = (value) => (value === "default" ? "prompt" : value);

export default function registerPermissions(context) {
    const media = { secureContext: true, api: "navigator.mediaDevices.getUserMedia" };
    context.registerPermission({ name: "camera", capability: "the camera", needs: { ...media, permissionsPolicy: "camera" }, query: "camera", request: () => requestMedia({ video: true }, "The device has no camera.") });
    context.registerPermission({ name: "microphone", capability: "the microphone", needs: { ...media, permissionsPolicy: "microphone" }, query: "microphone", request: () => requestMedia({ audio: true }, "The device has no microphone.") });
    context.registerPermission({ name: "location", capability: "the location", needs: { secureContext: true, api: "navigator.geolocation", permissionsPolicy: "geolocation" }, query: "geolocation", request: requestLocation });
    context.registerPermission({ name: "notifications", capability: "notifications", needs: { secureContext: true, api: "Notification" }, state: () => notificationState(Notification.permission), request: async () => notificationState(await Notification.requestPermission()) });
}
