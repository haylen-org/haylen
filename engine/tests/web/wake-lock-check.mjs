// Opens the page of the wake lock check in a headless browser over its debugging protocol and follows the wake lock of the page through its steps, then prints what happened as JSON. With "granted" the browser grants the lock, and the check expects the lock while the app keeps the screen on, its release when the page hides, a new lock when the page shows again and its release when the app lets the screen sleep. With "refused" the browser refuses every request and with "missing" it has no Screen Wake Lock API, and the check expects a warning in the log of the page while the app runs on.
// Run it with Node: node wake-lock-check.mjs <debugging port> <page address> <granted|refused|missing>
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

// The page reports every lock that the browser grants and every release of one in its log, and the other modes refuse every request or take the API away.
const watch = {
    granted: `const request = WakeLock.prototype.request;
        WakeLock.prototype.request = function (type) {
            return request.call(this, type).then((lock) => {
                console.log("The wake lock check saw the lock taken.");
                lock.addEventListener("release", () => console.log("The wake lock check saw the lock released."));
                return lock;
            });
        };`,
    refused: `WakeLock.prototype.request = function () {
            return Promise.reject(new DOMException("The browser refused the wake lock.", "NotAllowedError"));
        };`,
    missing: "delete Navigator.prototype.wakeLock;",
}[mode];

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await send("Runtime.enable", {}, sessionId);
await send("Page.enable", {}, sessionId);
await send("Page.addScriptToEvaluateOnNewDocument", { source: watch }, sessionId);
await send("Page.navigate", { url }, sessionId);

const pressSpace = async () => {
    for (const type of ["keyDown", "keyUp"]) {
        await send("Input.dispatchKeyEvent", { type, key: " ", code: "Space", windowsVirtualKeyCode: 32 }, sessionId);
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
};

if (await step("ready", "The wake lock check is ready", { seconds: 60 })) {
    if (mode === "granted") {
        await step("taken", "The wake lock check saw the lock taken.", { since: 0 });
        let since = result.log.length;
        const other = await send("Target.createTarget", { url: "about:blank" });
        await send("Target.activateTarget", { targetId: other.targetId });
        await step("released when hidden", "The wake lock check saw the lock released.", { since });
        since = result.log.length;
        await send("Target.activateTarget", { targetId: page.targetId });
        await send("Page.bringToFront", {}, sessionId);
        await step("taken when shown", "The wake lock check saw the lock taken.", { since });
        since = result.log.length;
        await pressSpace();
        await step("released when the app lets the screen sleep", "The wake lock check saw the lock released.", { since });
    } else {
        await step("warned", "keep the screen on", { since: 0 });
        const since = result.log.length;
        await pressSpace();
        await step("runs on", "The wake lock check lets the screen sleep.", { since });
    }
}
console.log(JSON.stringify(result));
process.exit(0);
