// Opens the page of the screens check in a headless browser over its debugging protocol, sets what the browser answers to the Window Management API, presses the key of the app that asks to see every screen and follows the log of the page, then prints what happened as JSON. With "granted" the app must see every screen, with "denied" the request must fail with the code "denied", and with "missing" the browser has no Window Management API and the request must fail with the code "unsupported".
// Run it with Node: node screens-check.mjs <debugging port> <page address> <granted|denied|missing>
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
    const before = since;
    const reached = await new Promise((resolve) => {
        const check = () => result.log.slice(before).some((line) => line.includes(text));
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
await send("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, sessionId);
await send("Page.enable", {}, sessionId);
// The page takes the focus, as the window that a person types into has it, since the browser counts a key as an input of the person only on a page with the focus.
await send("Page.bringToFront", {}, sessionId);
if (mode === "missing") {
    await send("Page.addScriptToEvaluateOnNewDocument", { source: "delete window.getScreenDetails;" }, sessionId);
} else {
    await send("Browser.setPermission", { permission: { name: "window-management" }, setting: mode, origin: new URL(url).origin });
}
await send("Page.navigate", { url }, sessionId);

// A key of the debugging protocol counts as an input of the person, which the browser wants shortly before it asks for the screens.
const pressSpace = async () => {
    for (const type of ["keyDown", "keyUp"]) {
        await send("Input.dispatchKeyEvent", { type, key: " ", code: "Space", windowsVirtualKeyCode: 32 }, sessionId);
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
};

const answers = {
    granted: ["sees every screen", "The screens check sees every screen"],
    denied: ["refused", 'The screens check got the code "denied"'],
    missing: ["unsupported", 'The screens check got the code "unsupported"'],
};
if (await step("ready", "The screens check is ready", { seconds: 60 })) {
    const since = result.log.length;
    await pressSpace();
    const [name, text] = answers[mode];
    await step(name, text, { since });
}
console.log(JSON.stringify(result));
process.exit(0);
