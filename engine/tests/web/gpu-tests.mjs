// Opens the page of the GPU tests in a headless browser over its debugging protocol, prints what the tests print, copies the images of the scenes that differ from their references out of the files of the page into the output folder, and exits with the status of the tests once they finish, or with 1 when the page fails or the time runs out.
// Run it with Node: node gpu-tests.mjs <debugging port> <page address> <seconds> <output folder>
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

const [port, url, seconds, output] = process.argv.slice(2);
const target = await (await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: "PUT" })).json();
const socket = new WebSocket(target.webSocketDebuggerUrl);
const pending = new Map();
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
            finish(1);
        }
    } else if (message.method === "Runtime.exceptionThrown") {
        console.log(message.params.exceptionDetails.exception?.description ?? message.params.exceptionDetails.text);
        finish(1);
    }
};

await new Promise((resolve) => (socket.onopen = resolve));
await send("Runtime.enable");
await send("Page.navigate", { url });
const timeout = new Promise((resolve) => setTimeout(() => resolve(null), Number(seconds) * 1000));
const status = await Promise.race([finished, timeout]);
if (status === null) {
    console.log(`The GPU tests did not finish within ${seconds} seconds.`);
}

// The page writes the images under the same path that the output folder has on this machine.
const listing = `(() => {
    const files = {};
    const walk = (path) => {
        for (const name of FS.readdir(path).filter((entry) => entry !== "." && entry !== "..")) {
            const child = path + "/" + name;
            if (FS.isDir(FS.stat(child).mode)) {
                walk(child);
            } else {
                files[child.slice(${JSON.stringify(output)}.length + 1)] = btoa(Array.from(FS.readFile(child), (byte) => String.fromCharCode(byte)).join(""));
            }
        }
    };
    if (FS.analyzePath(${JSON.stringify(output)}).exists) {
        walk(${JSON.stringify(output)});
    }
    return files;
})()`;
const files = (await send("Runtime.evaluate", { expression: listing, returnByValue: true })).result?.value ?? {};
for (const [name, data] of Object.entries(files)) {
    const path = join(output, name);
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, Buffer.from(data, "base64"));
}
socket.close();
process.exit(status ?? 1);
