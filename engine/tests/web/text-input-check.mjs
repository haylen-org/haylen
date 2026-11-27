// Opens the page of the text input check in a headless browser over its debugging protocol, taps or clicks each field of the sheet of the app with a press and a release that arrive at once and types right after, then prints what happened as JSON. Each field must take the keys typed after its press, also the second field while the first one edits. With "touch" the browser emulates a phone with a touch screen, and with "mouse" a desktop window whose mouse rests on the surface of the sheet before each click.
// Run it with Node: `node text-input-check.mjs <debugging port> <page address> <touch|mouse>`.
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

// Waits for a line of the log of the page that holds the text, among the lines from the one at `since` on, and records whether it came within the seconds.
const step = async (name, text, { seconds = 10, since = result.log.length } = {}) => {
    const reached = await new Promise((resolve) => {
        const check = () => result.log.slice(since).some((line) => line.includes(text));
        if (check()) {
            resolve(true);
            return;
        }
        waiters.push({ text, resolve });
        setTimeout(() => resolve(check()), seconds * 1000);
    });
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
// The page takes the focus, as the window that a person types into has it.
await send("Page.bringToFront", {}, sessionId);

const touch = mode === "touch";
if (touch) {
    await send("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 5 }, sessionId);
    await send("Emulation.setDeviceMetricsOverride", { width: 390, height: 844, deviceScaleFactor: 3, mobile: true, screenOrientation: { type: "portraitPrimary", angle: 0 } }, sessionId);
} else {
    await send("Emulation.setDeviceMetricsOverride", { width: 1280, height: 800, deviceScaleFactor: 2, mobile: false }, sessionId);
}
await send("Page.navigate", { url }, sessionId);

const pause = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

// The places of the fields in the page, from the fields that the engine lists to the page, once both show and the sheet that rises with them rests.
const readFields = async () => {
    const expression = '(() => { const fields = Module.haylen?.textInput?.fields ?? []; const canvas = Module.canvas; const box = canvas.getBoundingClientRect(); const scale = box.width / canvas.width; return fields.map((field) => ({ x: box.left + (field.bounds[0] + field.bounds[2] / 2) * scale, y: box.top + (field.bounds[1] + field.bounds[3] / 2) * scale, top: box.top + field.bounds[1] * scale, bottom: box.top + (field.bounds[1] + field.bounds[3]) * scale })); })()';
    const deadline = Date.now() + 10000;
    let previous = "";
    let fields = [];
    while (Date.now() < deadline) {
        fields = (await send("Runtime.evaluate", { expression, returnByValue: true }, sessionId)).result.value ?? [];
        const shown = JSON.stringify(fields);
        if (fields.length === 2 && shown === previous) {
            return fields;
        }
        previous = shown;
        await pause(300);
    }
    return [];
};

// A press and a release that arrive at once, after the mouse rested on the surface of the sheet between the fields, where it hovers the surface.
const press = async (field, rest) => {
    if (touch) {
        await send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x: field.x, y: field.y }] }, sessionId);
        await send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] }, sessionId);
        return;
    }

    await send("Input.dispatchMouseEvent", { type: "mouseMoved", x: rest.x, y: rest.y }, sessionId);
    await pause(500);
    await send("Input.dispatchMouseEvent", { type: "mouseMoved", x: field.x, y: field.y }, sessionId);
    await send("Input.dispatchMouseEvent", { type: "mousePressed", x: field.x, y: field.y, button: "left", clickCount: 1 }, sessionId);
    await send("Input.dispatchMouseEvent", { type: "mouseReleased", x: field.x, y: field.y, button: "left", clickCount: 1 }, sessionId);
};

const type = async (text) => {
    for (const key of text) {
        const code = `Key${key.toUpperCase()}`;
        const keyCode = key.toUpperCase().charCodeAt(0);
        await send("Input.dispatchKeyEvent", { type: "keyDown", key, code, text: key, windowsVirtualKeyCode: keyCode }, sessionId);
        await send("Input.dispatchKeyEvent", { type: "keyUp", key, code, windowsVirtualKeyCode: keyCode }, sessionId);
    }
};

if (await step("ready", "The text input check is ready", { seconds: 60, since: 0 })) {
    const fields = await readFields();
    if (fields.length === 2) {
        const rest = { x: fields[0].x, y: (fields[0].bottom + fields[1].top) / 2 };
        let since = result.log.length;
        await press(fields[0], rest);
        await type("ana");
        await step("the first field took the keys", 'The fields hold "ana" and "".', { since });
        since = result.log.length;
        await press(fields[1], rest);
        await type("pw");
        await step("the second field took the keys", 'The fields hold "ana" and "pw".', { since });
    }
}
console.log(JSON.stringify(result));
process.exit(0);
