// Opens the page of an app in a headless browser over its debugging protocol and follows its start, then prints what happened as JSON: the graphics API that the app logged it draws through, whether the splash ended, which it does only after the first complete frame of the app, the failure that the splash shows, the errors that the page threw, every line of its console with its level, every state that the splash went through and when each file of the page downloaded. The check ends at the first error, once the app drew its first frames for a few more seconds without one, or once the seconds pass.
// Run it with Node: node app-start-check.mjs <debugging port> <page address> <seconds>
const [port, url, seconds] = process.argv.slice(2);
const result = { drawsThrough: null, firstFrame: false, splashFailure: null, exceptions: [], log: [], splash: [], resources: [], expired: false, stalled: false };
// The app goes on drawing for these seconds after its first complete frame, so an error of its first frames still reaches the report.
const settleSeconds = 3;
const drawsThrough = /The app draws through "([^"]+)"\./;
// The engine writes every line as its time and its level in brackets before its message, and the page prints its warnings and errors alike as errors of the console, so a line of the engine takes its own level and any other line the type of its console call.
const engineLine = /^\[[^\]]+\] \[(\w+)\] /;
const pending = new Map();
let id = 0;

const report = () => {
    console.log(JSON.stringify(result));
    process.exit(0);
};

// The debugging protocol answers every request, so a browser that stops answering ends the check after the seconds of the page and a minute more.
setTimeout(() => {
    result.stalled = true;
    report();
}, (Number(seconds) + 60) * 1000);

const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const socket = new WebSocket(version.webSocketDebuggerUrl);

const send = (method, params = {}, sessionId = undefined) =>
    new Promise((resolve) => {
        id += 1;
        pending.set(id, resolve);
        socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
    });

const pause = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    // The workers of the page, such as the one of the content store, log and fail like the page, so the check follows their console too.
    if (message.method === "Target.attachedToTarget" && message.params.targetInfo.type === "worker") {
        send("Runtime.enable", {}, message.params.sessionId);
    }
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? {});
        pending.delete(message.id);
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description).join(" ");
        const level = text.match(engineLine)?.[1] ?? message.params.type;
        result.log.push({ level, text });
        const drawn = text.match(drawsThrough);
        if (drawn && result.drawsThrough === null) {
            result.drawsThrough = drawn[1];
        }
    } else if (message.method === "Runtime.exceptionThrown") {
        const details = message.params.exceptionDetails;
        const description = details.exception?.description ?? details.text;
        result.exceptions.push(`${description} at ${details.url ?? "the page"}:${details.lineNumber}:${details.columnNumber}`);
    }
};

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await send("Runtime.enable", {}, sessionId);
await send("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, sessionId);
await send("Page.enable", {}, sessionId);
// This script runs in the page before any of its own and records every state of the splash as it changes: the text of the percentage, whether the content of the splash still waits to show, whether the splash ended, whether its logo shows, how many images it shows and whether its bar shows. The page keeps the timing of every file it downloads, more than the browser keeps by default.
const splashRecorder = `(() => {
    const states = [];
    globalThis.splashStates = states;
    performance.setResourceTimingBufferSize(100000);
    const shown = (element) => element !== null && !element.hidden && getComputedStyle(element).display !== "none";
    const record = () => {
        const splash = document.getElementById("splash");
        if (splash === null) {
            return;
        }

        const logo = document.getElementById("splash-logo");
        const state = {
            percent: document.getElementById("splash-percent")?.textContent ?? null,
            starting: splash.classList.contains("starting"),
            ended: splash.classList.contains("hidden"),
            logo: logo === null ? null : shown(logo) && logo.complete !== false && logo.naturalWidth !== 0,
            images: [...splash.querySelectorAll("img, svg")].filter(shown).length,
            bar: shown(document.getElementById("splash-progress")),
        };
        const last = states[states.length - 1];
        if (last === undefined || JSON.stringify(last) !== JSON.stringify(state)) {
            states.push(state);
        }
    };
    new MutationObserver(record).observe(document, { subtree: true, childList: true, characterData: true, attributes: true });
})();`;
await send("Page.addScriptToEvaluateOnNewDocument", { source: splashRecorder }, sessionId);
await send("Page.navigate", { url }, sessionId);

// The splash of the page fades out once the app drew its first complete frame, and shows the failure of a start that never got that far.
const splashState = `JSON.stringify({
    ended: document.getElementById("splash")?.classList.contains("hidden") ?? false,
    failed: document.getElementById("splash")?.classList.contains("failed") ?? false,
    status: document.getElementById("splash-status")?.textContent ?? "",
    details: document.getElementById("splash-details")?.textContent ?? "",
})`;
const failed = () => result.exceptions.length > 0 || result.log.some((entry) => entry.level === "error");
const deadline = Date.now() + Number(seconds) * 1000;
for (;;) {
    if (failed()) {
        break;
    }

    if (Date.now() > deadline) {
        result.expired = true;
        break;
    }

    const state = JSON.parse((await send("Runtime.evaluate", { expression: splashState, returnByValue: true }, sessionId)).result?.value ?? "{}");
    if (state.failed) {
        result.splashFailure = { status: state.status, details: state.details };
        break;
    }

    if (state.ended && result.drawsThrough !== null) {
        result.firstFrame = true;
        await pause(settleSeconds * 1000);
        break;
    }

    await pause(250);
}

const loading = `JSON.stringify({
    splash: globalThis.splashStates ?? [],
    resources: performance.getEntriesByType("resource").map((entry) => ({ url: entry.name, start: entry.startTime, end: entry.responseEnd })),
})`;
Object.assign(result, JSON.parse((await send("Runtime.evaluate", { expression: loading, returnByValue: true }, sessionId)).result?.value ?? "{}"));
report();
