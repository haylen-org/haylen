// Opens the page of the error screen check in a headless browser over its debugging protocol, stops the app with an error, copies the report of the error screen and restarts the app from it, then prints what happened as JSON. The page address names the graphics API with `?graphicsApi=`, which the app must draw through, since a restart makes a new GPU device. With "granted" the browser lets the page write the clipboard, and the check expects the report on the clipboard, and the app drawn again after the key R and after a click on the restart button. With "refused" the browser refuses every write and with "missing" it has no clipboard API, and the check expects a warning in the log of the page, an untouched clipboard and the app drawn again after the key R. With "unknownApi" the address names a graphics API that the engine does not know, and the check expects its reason in the log of the page and in the failure that the page shows, and no exception.
// Run it with Node: node error-screen-check.mjs <debugging port> <page address> <granted|refused|missing|unknownApi>
const [port, url, mode] = process.argv.slice(2);
const api = new URL(url).searchParams.get("graphicsApi");
const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const socket = new WebSocket(version.webSocketDebuggerUrl);
const pending = new Map();
const waiters = [];
const result = { steps: [], failure: null, log: [] };
const width = 960;
const height = 540;
const errorBackground = [0x16, 0x19, 0x25];
const buttonColor = [0x2e, 0x34, 0x47];
const magenta = [255, 0, 255];
let id = 0;

const send = (method, params = {}, sessionId = undefined) =>
    new Promise((resolve) => {
        id += 1;
        pending.set(id, resolve);
        socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
    });

const pause = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

// Waits for a line of the log of the page that holds the text, among the lines from the one at `since` on, and returns whether it came within the seconds.
const logged = (text, { seconds = 10, since = result.log.length } = {}) =>
    new Promise((resolve) => {
        const check = () => result.log.slice(since).some((line) => line.includes(text));
        if (check()) {
            resolve(true);
            return;
        }
        waiters.push({ text, resolve });
        setTimeout(() => resolve(check()), seconds * 1000);
    });

const step = (name, reached) => {
    result.steps.push({ name, reached });
    return reached;
};

socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? {});
        pending.delete(message.id);
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description).join(" ");
        result.log.push(text);
        for (const waiter of waiters.splice(0)) {
            if (text.includes(waiter.text)) {
                waiter.resolve(true);
            } else {
                waiters.push(waiter);
            }
        }
    } else if (message.method === "Runtime.exceptionThrown") {
        result.failure = message.params.exceptionDetails.exception?.description ?? message.params.exceptionDetails.text;
    }
};

// The other modes refuse every write of the clipboard or take its API away.
const watch = {
    granted: "",
    refused: `Clipboard.prototype.writeText = function () {
            return Promise.reject(new DOMException("The browser refused the clipboard.", "NotAllowedError"));
        };`,
    missing: "delete Navigator.prototype.clipboard;",
}[mode];

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
const origin = new URL(url).origin;
await send("Browser.grantPermissions", { origin, permissions: ["clipboardReadWrite", "clipboardSanitizedWrite"] });
await send("Runtime.enable", {}, sessionId);
await send("Page.enable", {}, sessionId);
await send("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: 1, mobile: false }, sessionId);
await send("Emulation.setFocusEmulationEnabled", { enabled: true }, sessionId);
await send("Page.addScriptToEvaluateOnNewDocument", { source: watch }, sessionId);
await send("Page.navigate", { url }, sessionId);

const evaluate = async (expression) => (await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true }, sessionId)).result?.value;

const press = async (key, code, keyCode) => {
    for (const type of ["keyDown", "keyUp"]) {
        await send("Input.dispatchKeyEvent", { type, key, code, windowsVirtualKeyCode: keyCode }, sessionId);
        await pause(100);
    }
};

const click = async (x, y) => {
    for (const type of ["mouseMoved", "mousePressed", "mouseReleased"]) {
        await send("Input.dispatchMouseEvent", { type, x, y, button: type === "mouseMoved" ? "none" : "left", clickCount: 1 }, sessionId);
        await pause(100);
    }
};

// Reads the screenshot of the page in the page itself, which decodes it, and returns its pixels as rows of RGB values.
const capture = async () => {
    const { data } = await send("Page.captureScreenshot", { format: "png" }, sessionId);
    const pixels = await evaluate(`(async () => {
        const image = await createImageBitmap(await (await fetch("data:image/png;base64,${data}")).blob());
        const canvas = new OffscreenCanvas(image.width, image.height);
        const context = canvas.getContext("2d");
        context.drawImage(image, 0, 0);
        return Array.from(context.getImageData(0, 0, image.width, image.height).data);
    })()`);
    return (x, y) => pixels.slice((y * width + x) * 4, (y * width + x) * 4 + 3);
};

const near = (pixel, color) => pixel.every((value, index) => Math.abs(value - color[index]) <= 4);

// The canvas shows a color once the frames drew it, which the check waits for over a few seconds. The top right corner shows the background of the app and of the error screen alike.
const shows = async (color) => {
    for (let attempt = 0; attempt < 20; attempt += 1) {
        const pixel = await capture();
        if (near(pixel(width - 4, 4), color)) {
            return true;
        }
        await pause(250);
    }
    return false;
};

// The buttons of the error screen lie along its bottom edge, so the restart button is the second run of their color along a row just below their top edge.
const findRestartButton = async () => {
    const pixel = await capture();
    let bottom = height - 1;
    while (bottom > 0 && !near(pixel(30, bottom), buttonColor)) {
        bottom -= 1;
    }
    let top = bottom;
    while (top > 0 && near(pixel(30, top - 1), buttonColor)) {
        top -= 1;
    }
    const runs = [];
    for (let x = 0; x < width; x += 1) {
        if (!near(pixel(x, top + 1), buttonColor)) {
            continue;
        }
        if (runs.length > 0 && runs.at(-1).end === x - 1) {
            runs.at(-1).end = x;
        } else {
            runs.push({ start: x, end: x });
        }
    }
    const button = runs.filter((run) => run.end - run.start > 20)[1];
    return button ? { x: (button.start + button.end) / 2, y: (top + bottom) / 2 } : null;
};

const clipboard = () => evaluate("navigator.clipboard ? navigator.clipboard.readText() : ''");

// The app stops once E is pressed, and the error screen replaces its magenta.
const stop = async (name) => {
    const since = result.log.length;
    await press("e", "KeyE", 69);
    return step(name, (await logged("The error screen check stops the app.", { since })) && (await shows(errorBackground)));
};

const restart = async (name, action) => {
    const since = result.log.length;
    await action();
    return step(name, (await logged("The error screen check is ready.", { since })) && (await shows(magenta)));
};

const pressR = () => press("r", "KeyR", 82);

// A page that names a graphics API the engine does not know never starts the app, and the reason reaches the log and the page, over the splash or on the error screen, with no exception of its own.
if (mode === "unknownApi") {
    const reason = `The graphics API "${api}" of "--graphics-api" is unknown.`;
    step("reported", await logged(reason, { seconds: 60, since: 0 }));
    // The loader shows the reason over its splash when the error comes before the start, and the error screen of the engine shows it on the canvas once the splash ends.
    step("shown", (await evaluate("document.body.innerText")).includes(reason) || (await shows(errorBackground)));
    console.log(JSON.stringify(result));
    process.exit(0);
}

const ready = (await logged("The error screen check is ready.", { seconds: 60, since: 0 })) && (await logged(`The app draws through "${api}".`, { since: 0 }));
if (step("ready", ready && (await shows(magenta))) && (await stop("stopped"))) {
    if (mode === "granted") {
        await press("c", "KeyC", 67);
        await pause(500);
        const copied = await clipboard();
        step("copied", copied.startsWith("The app stopped with an error") && copied.includes("The error screen check stops the app."));
        if ((await restart("restarted by the key", pressR)) && (await stop("stopped again"))) {
            const button = await findRestartButton();
            await restart("restarted by a click", () => (button ? click(button.x, button.y) : pause(0)));
        }
    } else {
        const since = result.log.length;
        await press("c", "KeyC", 67);
        const warned = await logged(mode === "refused" ? "The browser refused to write the clipboard" : "The page cannot write the clipboard", { since });
        step("refused", warned && !(await clipboard()).includes("The error screen check stops the app."));
        await restart("restarted by the key", pressR);
    }
}
console.log(JSON.stringify(result));
process.exit(0);
