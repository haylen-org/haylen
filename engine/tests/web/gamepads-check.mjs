// Opens the page of the gamepads check in a headless browser over its debugging protocol, gives the page a Gamepad API whose gamepads the driver connects, presses, disconnects and connects again the way a browser reports them, and follows the log of the page, then prints what happened as JSON. Two gamepads press different face buttons, so the app must see each one in its own slot, keep the second one in its slot while the first one is gone and see the first one back in its slot once it connects again.
// Run it with Node: `node gamepads-check.mjs <debugging port> <page address>`.
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

// The Gamepad API of the page lists the gamepads of `haylenPads` by their index, as a browser lists the gamepads it keeps, and tells the page of each connection and disconnection through the events of the window. A gamepad connects with no button down and then presses a face button of the standard mapping, and its timestamp moves on at every read, as the gamepad of a browser does while it reports.
const gamepadApi = `
    window.haylenPads = [null, null, null, null];
    Object.defineProperty(navigator, "getGamepads", { configurable: true, value: () => window.haylenPads.slice() });
    const announce = (type, pad) => {
        const event = new Event(type);
        Object.defineProperty(event, "gamepad", { value: pad });
        window.dispatchEvent(event);
    };
    window.haylenConnectPad = (index, name) => {
        const pad = {
            index,
            id: name + " (STANDARD GAMEPAD Vendor: 054c Product: 09cc)",
            connected: true,
            mapping: "standard",
            buttons: Array.from({ length: 17 }, () => ({ pressed: false, touched: false, value: 0 })),
            axes: [0, 0, 0, 0],
        };
        Object.defineProperty(pad, "timestamp", { get: () => performance.now() });
        window.haylenPads[index] = pad;
        announce("gamepadconnected", pad);
    };
    window.haylenPress = (index, button) => {
        window.haylenPads[index].buttons[button] = { pressed: true, touched: true, value: 1 };
    };
    window.haylenDisconnectPad = (index) => {
        const pad = window.haylenPads[index];
        pad.connected = false;
        window.haylenPads[index] = null;
        announce("gamepaddisconnected", pad);
    };
`;

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await send("Runtime.enable", {}, sessionId);
await send("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, sessionId);
await send("Page.enable", {}, sessionId);
await send("Page.addScriptToEvaluateOnNewDocument", { source: gamepadApi }, sessionId);
await send("Page.navigate", { url }, sessionId);

const evaluate = (expression) => send("Runtime.evaluate", { expression }, sessionId);

// The button 0 of the standard mapping is the south button and the button 1 the east button.
if (await step("ready", "The gamepads check is ready", { seconds: 60 })) {
    let since = result.log.length;
    await evaluate('haylenConnectPad(0, "First Pad"); haylenConnectPad(1, "Second Pad");');
    if (await step("connected two gamepads", "The gamepads check sees slot 1 idle, slot 2 idle", { since })) {
        since = result.log.length;
        await evaluate("haylenPress(0, 0); haylenPress(1, 1);");
        if (await step("told two gamepads apart", "The gamepads check sees slot 1 south, slot 2 east", { since })) {
            since = result.log.length;
            await evaluate("haylenDisconnectPad(0);");
            if (await step("kept the other in its slot", "The gamepads check sees slot 1 free, slot 2 east", { since })) {
                since = result.log.length;
                await evaluate('haylenConnectPad(0, "First Pad");');
                if (await step("reconnected in its place", "The gamepads check sees slot 1 idle, slot 2 east", { since })) {
                    since = result.log.length;
                    await evaluate("haylenPress(0, 0);");
                    await step("pressed again in its place", "The gamepads check sees slot 1 south, slot 2 east", { since });
                }
            }
        }
    }
}
console.log(JSON.stringify(result));
process.exit(0);
