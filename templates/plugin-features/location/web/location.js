// The location of the {{TITLE}} plugin on the web: one fix of `navigator.geolocation`, or every change while the app watches, sent batched as the event `located`, once the page holds the permission `location`, which the browser offers only to secure pages.
const failure = (message, code) => Object.assign(new Error(message), { code });

const place = (position) => ({ latitude: position.coords.latitude, longitude: position.coords.longitude, accuracy: position.coords.accuracy });

const failed = (error) => failure(`The location is unknown: ${error.message}`, "locationUnknown");

export default function registerLocation(context) {
    let watch = null;

    const stopWatching = () => {
        if (watch !== null) {
            navigator.geolocation.clearWatch(watch);
            watch = null;
        }
    };

    context.register("locate", async () => {
        await context.requirePermission("location");
        return new Promise((resolve, reject) => navigator.geolocation.getCurrentPosition((position) => resolve(place(position)), (error) => reject(failed(error))));
    });

    context.register("watchLocation", async () => {
        await context.requirePermission("location");
        stopWatching();
        watch = navigator.geolocation.watchPosition((position) => context.emit("located", place(position), { batched: true }), (error) => console.warn(failed(error).message));
        return null;
    });

    context.register("stopWatchingLocation", () => {
        stopWatching();
        return null;
    });
}
