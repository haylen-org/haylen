// Opens the page of the GPU tests in a headless browser over its debugging protocol, prints what the tests print, writes the images of the scenes that differ from their references, which the page kept, into the output folder, and writes the report of the page as JSON: the status that the tests finished with, or null when they did not finish within the time, and every stop and uncaught exception of the page, whenever it came.
// Run it with Node: node gpu-tests.mjs <debugging port> <page address> <seconds> <output folder> <report file>
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

const [port, url, seconds, output, report] = process.argv.slice(2);
const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: "PUT" })).json();
const socket = new WebSocket(target.webSocketDebuggerUrl);
const pending = new Map();
const failures = [];
let id = 0;
let finish;
const finished = new Promise((resolve) => (finish = resolve));

const send = (method, params = {}) =>
    new Promise((resolve) => {
        id += 1;
        pending.set(id, resolve);
        socket.send(JSON.stringify({ id, method, params }));
    });

socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? {});
        pending.delete(message.id);
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description).join(" ");
        console.log(text);
        const status = /^The GPU tests finished with the status (\d+)\.$/.exec(text);
        if (status) {
            finish(Number(status[1]));
        } else if (text.startsWith("The GPU tests stopped:")) {
            failures.push(text);
            finish(null);
        }
    } else if (message.method === "Runtime.exceptionThrown") {
        const text = message.params.exceptionDetails.exception?.description ?? message.params.exceptionDetails.text;
        console.log(text);
        failures.push(text);
        finish(null);
    }
};

await new Promise((resolve) => (socket.onopen = resolve));
await send("Runtime.enable");
await send("Page.navigate", { url });
const timeout = new Promise((resolve) => setTimeout(() => resolve(null), Number(seconds) * 1000));
const status = await Promise.race([finished, timeout]);

// The page wrote the images under the same path that the output folder has on this machine, and kept them in a plain object as its runtime exited.
const differences = (await send("Runtime.evaluate", { expression: "differences", returnByValue: true })).result?.value ?? {};
for (const [name, data] of Object.entries(differences)) {
    const path = join(output, name);
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, Buffer.from(data, "base64"));
}
socket.close();
writeFileSync(report, JSON.stringify({ status, failures }));
process.exit(0);
