// Opens the page of the canvas check in a headless browser over its debugging protocol as a desktop window of density 2 and resizes the page, then prints what happened as JSON. The canvas must fill the page each time, with a framebuffer of the pixels of the page, which the app logs. With "first" the page draws through the first graphics API that starts, and with "fallback" the browser offers no WebGPU adapter, so the window of WebGPU closes and the window of WebGL2 opens on the same canvas.
// Run it with Node: node canvas-check.mjs <debugging port> <page address> <first|fallback>
const [port, url, mode] = process.argv.slice(2);
const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const socket = new WebSocket(version.webSocketDebuggerUrl);
const pending = new Map();
const waiters = [];
const result = { steps: [], warnings: [], failure: null, log: [] };
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
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? {});
        pending.delete(message.id);
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description).join(" ");
        result.log.push(text);
        if (message.params.type === "warning") {
            result.warnings.push(text);
        } else if (message.params.type === "error") {
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
await send("Page.enable", {}, sessionId);

// A desktop window without touch, whose size the check changes.
const size = (width, height, density) => send("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: density, mobile: false }, sessionId);
await size(1440, 900, 2);
if (mode === "fallback") {
    await send("Page.addScriptToEvaluateOnNewDocument", { source: "if (window.GPU) { GPU.prototype.requestAdapter = async () => null; }" }, sessionId);
}
await send("Page.navigate", { url }, sessionId);

// Reads the box of the canvas in the page and the pixels of its drawable.
const canvas = async () =>
    (await send("Runtime.evaluate", { expression: '(() => { const canvas = document.getElementById("canvas"); const box = canvas.getBoundingClientRect(); return { width: box.width, height: box.height, pixelWidth: canvas.width, pixelHeight: canvas.height }; })()', returnByValue: true }, sessionId)).result.value;

// Waits until the canvas covers the whole page with a drawable of every pixel of the page and the app logs that framebuffer, and records whether that happened within the seconds, with what the canvas showed otherwise.
const fills = async (name, width, height, density, since, seconds = 10) => {
    const pixelWidth = width * density;
    const pixelHeight = height * density;
    const logged = await waitForLine(`The canvas check draws ${pixelWidth} by ${pixelHeight} pixels.`, since, seconds);
    const deadline = Date.now() + seconds * 1000;
    let shown = await canvas();
    const filled = () => shown.width === width && shown.height === height && shown.pixelWidth === pixelWidth && shown.pixelHeight === pixelHeight;
    while (Date.now() < deadline && !filled()) {
        await new Promise((resolve) => setTimeout(resolve, 200));
        shown = await canvas();
    }
    result.steps.push({ name, reached: logged && filled() });
    if (!filled()) {
        result.log.push(`The canvas showed ${shown.width} by ${shown.height} points with ${shown.pixelWidth} by ${shown.pixelHeight} pixels on a page of ${width} by ${height} points at density ${density}.`);
    }
};

if (await step("ready", "The canvas check is ready", { seconds: 60, since: 0 })) {
    if (mode === "fallback") {
        await step("fell back to WebGL2", 'The app draws through "webGl2"', { since: 0 });
    }
    await fills("fills the page", 1440, 900, 2, 0);
    let since = result.log.length;
    await size(1200, 760, 2);
    await fills("follows a resize", 1200, 760, 2, since);
}
console.log(JSON.stringify(result));
process.exit(0);
