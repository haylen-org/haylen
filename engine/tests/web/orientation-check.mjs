// Opens the page of the orientation check in a headless browser that emulates a phone over its debugging protocol, turns the phone and presses the keys of the app that lock the orientation, then prints what happened as JSON. A phone held in portrait shows the turn screen of the page over an app that takes landscape while the app runs on under it, the screen goes away once the phone turns, and the lock of the app replaces the orientation of "haylen.json".
// Run it with Node: `node orientation-check.mjs <debugging port> <page address>`.
const [port, url] = process.argv.slice(2);
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
await send("Page.bringToFront", {}, sessionId);

// The phone has a touch screen without hover, which the page takes for a device that turns, and starts in portrait.
const hold = (orientation) => {
    const portrait = orientation === "portrait";
    return send("Emulation.setDeviceMetricsOverride", { width: portrait ? 390 : 844, height: portrait ? 844 : 390, deviceScaleFactor: 2, mobile: true, screenOrientation: { type: portrait ? "portraitPrimary" : "landscapePrimary", angle: portrait ? 0 : 90 } }, sessionId);
};
await send("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 5 }, sessionId);
await hold("portrait");
await send("Page.navigate", { url }, sessionId);

const press = async (key) => {
    const code = `Key${key.toUpperCase()}`;
    const keyCode = key.toUpperCase().charCodeAt(0);
    for (const type of ["keyDown", "keyUp"]) {
        await send("Input.dispatchKeyEvent", { type, key, code, windowsVirtualKeyCode: keyCode }, sessionId);
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
};

// Reads the turn screen of the page: whether it shows, and the orientation that its caption asks for.
const turnScreen = async () =>
    (await send("Runtime.evaluate", { expression: '(() => { const turn = document.getElementById("turn"); return { shown: !turn.hidden, caption: document.getElementById("turn-caption").textContent }; })()', returnByValue: true }, sessionId)).result.value;

// Waits until the turn screen shows or hides, with the caption that names the orientation when it shows, and records whether it did within the seconds.
const screenState = async (name, shown, orientation, seconds = 10) => {
    const deadline = Date.now() + seconds * 1000;
    let state = await turnScreen();
    while (Date.now() < deadline && !(state.shown === shown && (!shown || state.caption.includes(orientation)))) {
        await new Promise((resolve) => setTimeout(resolve, 200));
        state = await turnScreen();
    }
    result.steps.push({ name, reached: state.shown === shown && (!shown || state.caption.includes(orientation)) });
};

if (await step("ready", "The orientation check is ready", { seconds: 60 })) {
    await screenState("shown in portrait", true, "landscape");
    await step("runs under it", "The orientation check drew", { since: result.log.length });
    await hold("landscape");
    await screenState("hidden in landscape", false);
    let since = result.log.length;
    await press("p");
    await step("locked portrait", "locked portrait", { since });
    await screenState("shown for the lock", true, "portrait");
    since = result.log.length;
    await press("a");
    await step("turns freely", "lets the screen turn", { since });
    await screenState("hidden for any", false);
}
console.log(JSON.stringify(result));
process.exit(0);
