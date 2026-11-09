// Opens the page of the plugin check in a headless browser over its debugging protocol, with a permissions policy that governs only the feature "camera" and refuses it, and prints what happened as JSON. The plugin of the app requires "camera" and "web-share", and the check expects the page to lack "camera" alone, since a feature that the policy of the browser does not govern is left to the error of its API. Then the plugin throws an error of the page, which must fail its call with the code "exception" and the name of the error in "data.type", and requires a permission whose request ends in "prompt", which must fail with the code "denied" and the sentence that says the system did not show its prompt. Last it requires a permission that it never registered, which must fail with the code "unsupported" and the sentence of the other platforms.
// Run it with Node: node plugin-check.mjs <debugging port> <page address>
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

// Waits for a line of the log of the page that holds the text, and records whether it came within the seconds.
const step = async (name, text, seconds) => {
    const reached = await new Promise((resolve) => {
        const check = () => result.log.some((line) => line.includes(text));
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

// The policy of the page governs "camera" alone and refuses it, as a page in a frame without "allow=camera" sees it, and like the policies of browsers it allows no feature that it does not govern.
const policy = `{
        const policy = { features: () => ["camera"], allowedFeatures: () => [], allowsFeature: () => false };
        Object.defineProperty(Document.prototype, "permissionsPolicy", { get: () => policy });
        Object.defineProperty(Document.prototype, "featurePolicy", { get: () => policy });
    }`;

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await send("Runtime.enable", {}, sessionId);
await send("Page.enable", {}, sessionId);
await send("Page.addScriptToEvaluateOnNewDocument", { source: policy }, sessionId);
await send("Page.navigate", { url }, sessionId);

if (await step("checked", "The plugin check found these features missing:", 60)) {
    await step("lacks only the governed feature", "The plugin check found these features missing: camera.", 1);
}
await step("fails an error of the page as an exception", 'The error of the page failed with the code "exception" and the type "NotAllowedError".', 10);
await step("fails a use without a prompt as denied", 'The use without a prompt failed with the code "denied": The system did not show its prompt for the unprompted capability, so the app does not hold it yet. Ask for it again later with "system.requestPermission(\'unprompted\')".', 10);
await step("fails an unregistered permission as unsupported", 'The use of an unregistered permission failed with the code "unsupported": The plugin "page-plugin" requires the permission "unregistered", which it did not register with "registerPermission".', 10);
console.log(JSON.stringify(result));
process.exit(0);
