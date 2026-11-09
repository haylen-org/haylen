// Opens the page of the fullscreen check in a headless browser over its debugging protocol, presses the keys of the app that change fullscreen and follows the log of the page, then prints what happened as JSON. Each promise of the app must settle once the window reports the mode it asked for, and a change that a newer one replaces must fail with the code "denied" while the page ends in the mode of the newer change.
// Run it with Node: node fullscreen-check.mjs <debugging port> <page address>
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
await send("Page.navigate", { url }, sessionId);

// A key of the debugging protocol counts as an input of the person, which the browser wants shortly before it goes fullscreen.
const press = async (key) => {
    const code = `Key${key.toUpperCase()}`;
    const keyCode = key.toUpperCase().charCodeAt(0);
    for (const type of ["keyDown", "keyUp"]) {
        await send("Input.dispatchKeyEvent", { type, key, code, windowsVirtualKeyCode: keyCode }, sessionId);
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
};

const pageFullscreen = async () => (await send("Runtime.evaluate", { expression: "document.fullscreenElement !== null", returnByValue: true }, sessionId)).result.value;

if (await step("ready", "The fullscreen check is ready", { seconds: 60 })) {
    let since = result.log.length;
    await press("f");
    await step("entered", 'saw "enter" settle, and the window reports true.', { since });
    await step("heard", "heard the window change to true.", { since });
    since = result.log.length;
    await press("l");
    await step("left", 'saw "leave" settle, and the window reports false.', { since });
    since = result.log.length;
    await press("r");
    await step("replaced", 'saw "replaced" fail with "denied"', { since });
    if (await step("replacing", 'saw "replacing" settle, and the window reports false.', { since })) {
        await new Promise((resolve) => setTimeout(resolve, 1000));
        result.steps.push({ name: "ends windowed", reached: !(await pageFullscreen()) });
    }
}
console.log(JSON.stringify(result));
process.exit(0);
