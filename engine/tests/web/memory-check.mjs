// Opens the page of the memory check of the web in a headless browser over its debugging protocol, follows the page and its workers until the app logs that it downloaded the large file or failed, and prints what it measured as JSON: the bytes that the page and its workers received, whether the medium file came down, the peak of the memory of WebAssembly, and the memory behind the array buffers of the page and its workers once the browser collected their garbage. With "memory" the page runs as in a browser that keeps no files for it.
// Run it with Node: node memory-check.mjs <debugging port> <page address> <seconds> <files|memory>
const [port, url, seconds, store] = process.argv.slice(2);
const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: "PUT" })).json();
const socket = new WebSocket(target.webSocketDebuggerUrl);
const pending = new Map();
const sessions = new Set([""]);
const result = { receivedBytes: 0, peakWasmBytes: 0, buffersAfterCollection: 0, downloadedMedium: false, finished: false, failure: null, log: [] };
let id = 0;

const send = (method, params = {}, sessionId = "") =>
    new Promise((resolve) => {
        id += 1;
        pending.set(id, resolve);
        socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
    });

const follow = async (sessionId) => {
    sessions.add(sessionId);
    await send("Runtime.enable", {}, sessionId);
    await send("Network.enable", {}, sessionId);
    await send("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, sessionId);
    await send("Runtime.runIfWaitingForDebugger", {}, sessionId);
};

socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? {});
        pending.delete(message.id);
    } else if (message.method === "Target.attachedToTarget") {
        follow(message.params.sessionId);
    } else if (message.method === "Network.loadingFinished") {
        result.receivedBytes += message.params.encodedDataLength;
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description).join(" ");
        result.log.push(text);
        if (text.includes("The memory check downloaded the medium file")) {
            result.downloadedMedium = true;
        } else if (text.includes("The memory check downloaded the large file")) {
            result.finished = true;
        } else if (message.params.type === "error") {
            result.failure = text;
        }
    } else if (message.method === "Runtime.exceptionThrown") {
        result.failure = message.params.exceptionDetails.exception?.description ?? message.params.exceptionDetails.text;
    }
};

await new Promise((resolve) => (socket.onopen = resolve));
await follow("");
if (store === "memory") {
    await send("Page.enable");
    await send("Page.addScriptToEvaluateOnNewDocument", { source: "delete StorageManager.prototype.getDirectory;" });
}
await send("Page.navigate", { url });

// The memory of WebAssembly only grows, so its size four times a second gives its peak.
const deadline = Date.now() + Number(seconds) * 1000;
while (!result.finished && !result.failure && Date.now() < deadline) {
    const memory = await send("Runtime.evaluate", { expression: "typeof HEAPU8 === 'undefined' ? 0 : HEAPU8.length", returnByValue: true });
    result.peakWasmBytes = Math.max(result.peakWasmBytes, memory.result?.value ?? 0);
    await new Promise((resolve) => setTimeout(resolve, 250));
}

// Array buffers that nothing holds stay until the browser collects them, so the ones that the page and its workers keep show once it did.
for (const sessionId of sessions) {
    await send("HeapProfiler.collectGarbage", {}, sessionId);
    result.buffersAfterCollection += (await send("Runtime.getHeapUsage", {}, sessionId)).backingStorageSize ?? 0;
}
result.log = result.log.slice(-20);
console.log(JSON.stringify(result));
await fetch(`http://127.0.0.1:${port}/json/close/${target.id}`);
socket.close();
