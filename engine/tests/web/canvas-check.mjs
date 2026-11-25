// Opens the page of the canvas check in a headless browser over its debugging protocol as a desktop window of density 2, resizes the page and moves it to a screen of density 1, then prints what happened as JSON. The canvas must fill the page each time, with a framebuffer of the pixels of the page, which the app logs. With "first" the page draws through the first graphics API that starts, with "fallback" the browser offers no WebGPU adapter, so the window of WebGPU closes and the window of WebGL2 opens on the same canvas, with "worker" the page runs the app in the Worker mode, whose canvas the worker draws into, and with "noGraphics" the browser offers no WebGPU adapter and no WebGL2 context on the canvas of the app, so the splash of the page must show that no graphics API started.
// Run it with Node: node canvas-check.mjs <debugging port> <page address> <first|fallback|worker|noGraphics>
const [port, url, mode] = process.argv.slice(2);
const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const socket = new WebSocket(version.webSocketDebuggerUrl);
const pending = new Map();
const waiters = [];
const result = { steps: [], warnings: [], failure: null, log: [] };
// The reason that the runtime logs and the page reports when no graphics API starts, which a page that offers none must give and which is no failure of that page.
const noGraphicsReason = "No graphics API could start on this machine";
let id = 0;

const send = (method, params = {}, sessionId = undefined) =>
    new Promise((resolve) => {
        id += 1;
        pending.set(id, resolve);
        socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
    });

// Waits for a line of the log of the page that holds the text, among the lines from the one at `since` on, and answers whether it came within the seconds.
const waitForLine = (text, since, seconds) =>
    new Promise((resolve) => {
        const check = () => result.log.slice(since).some((line) => line.includes(text));
        if (check()) {
            resolve(true);
            return;
        }
        waiters.push({ text, resolve });
        setTimeout(() => resolve(check()), seconds * 1000);
    });

// Records whether a line of the log that holds the text came within the seconds.
const step = async (name, text, { seconds = 10, since = result.log.length } = {}) => {
    const reached = await waitForLine(text, since, seconds);
    result.steps.push({ name, reached });
    return reached;
};

socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    // The runtime of the Worker mode logs in its worker, whose console the check follows like the console of the page.
    if (message.method === "Target.attachedToTarget" && message.params.targetInfo.type === "worker") {
        send("Runtime.enable", {}, message.params.sessionId);
    }
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? {});
        pending.delete(message.id);
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description).join(" ");
        result.log.push(text);
        if (message.params.type === "warning") {
            result.warnings.push(text);
        } else if (message.params.type === "error" && !(mode === "noGraphics" && text.includes(noGraphicsReason))) {
            result.failure = text;
        }
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

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await send("Runtime.enable", {}, sessionId);
await send("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, sessionId);
await send("Page.enable", {}, sessionId);

// A desktop window without touch, whose size the check changes.
const size = (width, height, density) => send("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: density, mobile: false }, sessionId);
await size(1440, 900, 2);

// The debugging protocol changes the density of the page without the change of its resolution that a browser reports when a window moves to another screen, so the page keeps the media queries of the resolution that it makes, and the check reports that change to them itself once it changed the density.
const resolutions = "const pageMatchMedia = window.matchMedia.bind(window); window.haylenResolutionQueries = []; window.matchMedia = (text) => { const list = pageMatchMedia(text); if (text.includes('resolution')) { window.haylenResolutionQueries.push(list); } return list; };";
await send("Page.addScriptToEvaluateOnNewDocument", { source: resolutions }, sessionId);
const moveToScreen = async (width, height, density) => {
    await size(width, height, density);
    await send("Runtime.evaluate", { expression: 'for (const list of window.haylenResolutionQueries.splice(0)) { list.dispatchEvent(new Event("change")); }' }, sessionId);
};
if (mode === "fallback" || mode === "noGraphics") {
    await send("Page.addScriptToEvaluateOnNewDocument", { source: "if (window.GPU) { GPU.prototype.requestAdapter = async () => null; }" }, sessionId);
}
if (mode === "noGraphics") {
    // The loader of the page finds WebGL2 on a canvas of its own, so the page starts the runtime, whose canvas then gives no context, as a driver that refuses the attributes of the app does.
    const noWebGl = "const pageGetContext = HTMLCanvasElement.prototype.getContext; HTMLCanvasElement.prototype.getContext = function (type, ...options) { return type === 'webgl2' && this.id === 'canvas' ? null : pageGetContext.call(this, type, ...options); };";
    await send("Page.addScriptToEvaluateOnNewDocument", { source: noWebGl }, sessionId);
}
await send("Page.navigate", { url }, sessionId);

// Reads the box of the canvas in the page and the pixels of its drawable.
const canvas = async () =>
    (await send("Runtime.evaluate", { expression: '(() => { const canvas = document.getElementById("canvas"); const box = canvas.getBoundingClientRect(); return { width: box.width, height: box.height, pixelWidth: canvas.width, pixelHeight: canvas.height }; })()', returnByValue: true }, sessionId)).result.value;

// Waits until the canvas covers the whole page with a drawable of every pixel of the page and the app logs that framebuffer, and records whether that happened within the seconds, with what the canvas showed otherwise. In the Worker mode the page hands the drawable of its canvas to the worker, so there the framebuffer that the app logs tells the pixels.
const fills = async (name, width, height, density, since, seconds = 10) => {
    const pixelWidth = width * density;
    const pixelHeight = height * density;
    const logged = await waitForLine(`The canvas check draws ${pixelWidth} by ${pixelHeight} pixels.`, since, seconds);
    const deadline = Date.now() + seconds * 1000;
    let shown = await canvas();
    const drawn = () => mode === "worker" || (shown.pixelWidth === pixelWidth && shown.pixelHeight === pixelHeight);
    const filled = () => shown.width === width && shown.height === height && drawn();
    while (Date.now() < deadline && !filled()) {
        await new Promise((resolve) => setTimeout(resolve, 200));
        shown = await canvas();
    }
    result.steps.push({ name, reached: logged && filled() });
    if (!filled()) {
        result.log.push(`The canvas showed ${shown.width} by ${shown.height} points with ${shown.pixelWidth} by ${shown.pixelHeight} pixels on a page of ${width} by ${height} points at density ${density}.`);
    }
};

// The splash of the page shows the failure of a start that never reached the app, with the reason that the runtime reported.
const showsFailure = async (text, seconds) => {
    const deadline = Date.now() + seconds * 1000;
    const splash = 'JSON.stringify({ failed: document.getElementById("splash")?.classList.contains("failed") ?? false, details: document.getElementById("splash-details")?.textContent ?? "" })';
    while (Date.now() < deadline) {
        const state = JSON.parse((await send("Runtime.evaluate", { expression: splash, returnByValue: true }, sessionId)).result?.value ?? "{}");
        if (state.failed && state.details.includes(text)) {
            return true;
        }
        await new Promise((resolve) => setTimeout(resolve, 250));
    }
    return false;
};

if (mode === "noGraphics") {
    result.steps.push({ name: "showed the failure", reached: await showsFailure(noGraphicsReason, 60) });
} else if (await step("ready", "The canvas check is ready", { seconds: 60, since: 0 })) {
    if (mode === "fallback") {
        await step("fell back to WebGL2", 'The app draws through "webGl2"', { since: 0 });
    }
    await fills("fills the page", 1440, 900, 2, 0);
    let since = result.log.length;
    await size(1200, 760, 2);
    await fills("follows a resize", 1200, 760, 2, since);
    since = result.log.length;
    await moveToScreen(1200, 760, 1);
    await fills("follows the density", 1200, 760, 1, since);
}
console.log(JSON.stringify(result));
process.exit(0);
