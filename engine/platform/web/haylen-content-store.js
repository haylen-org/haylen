// The content store of a Haylen page, in the private file system of the origin, one folder per app under `haylen-content`. It runs in a worker, where files open with synchronous access handles, and it takes the changes of the runtime in the order they were made, each written before the next one starts. Writes of whole files flush at once, and appends to segments flush when the runtime syncs the segment, which it does before it commits a state, so the state the runtime commits last never reaches the disk before the records it names, and after a change that the browser refuses, such as one past the quota of the origin, it keeps no later change. A tab that finds the files of the store held by another tab of the same app answers that the store cannot open, and its runtime keeps its downloads in memory for that visit.

let root = null;
const handles = new Map();
let queue = Promise.resolve();

// Returns the folder and the name of a file of the store, which uses `/` between folders.
const locate = async (name, create) => {
    const parts = name.split("/");
    let folder = root;
    for (const part of parts.slice(0, -1)) {
        folder = await folder.getDirectoryHandle(part, { create });
    }
    return [folder, parts[parts.length - 1]];
};

const open = async (name, create) => {
    if (!handles.has(name)) {
        const [folder, file] = await locate(name, create);
        const handle = await (await folder.getFileHandle(file, { create })).createSyncAccessHandle();
        handles.set(name, handle);
    }
    return handles.get(name);
};

const close = (name) => {
    const handle = handles.get(name);
    if (handle) {
        handle.close();
        handles.delete(name);
    }
};

const available = async () => {
    const estimate = await navigator.storage.estimate();
    return Math.max(0, (estimate.quota || 0) - (estimate.usage || 0));
};

// Lists every file under a folder with its path from the root of the store.
const walk = async (folder, prefix, found) => {
    for await (const [name, entry] of folder.entries()) {
        if (entry.kind === "directory") {
            await walk(entry, prefix + name + "/", found);
        } else {
            found.push(prefix + name);
        }
    }
    return found;
};

const operations = {
    // Opens the folder of an app and answers with its small files whole and the sizes of its segments, which the runtime reads in only when it needs them.
    async open({ identifier }) {
        const storage = await navigator.storage.getDirectory();
        root = await (await storage.getDirectoryHandle("haylen-content", { create: true })).getDirectoryHandle(identifier, { create: true });
        const files = [];
        const segments = {};
        for (const name of await walk(root, "", [])) {
            const handle = await open(name, false);
            if (name.startsWith("segments/")) {
                segments[name] = handle.getSize();
                continue;
            }
            const bytes = new Uint8Array(handle.getSize());
            handle.read(bytes, { at: 0 });
            files.push({ name, bytes });
        }
        return { files, segments, available: await available() };
    },
    async write({ name, bytes }) {
        const handle = await open(name, true);
        handle.truncate(0);
        handle.write(bytes, { at: 0 });
        handle.flush();
    },
    async append({ name, bytes }) {
        const handle = await open(name, true);
        handle.write(bytes, { at: handle.getSize() });
    },
    async flush({ name }) {
        handles.get(name)?.flush();
    },
    async truncate({ name, size }) {
        const handle = await open(name, true);
        handle.truncate(size);
        handle.flush();
    },
    async remove({ name }) {
        close(name);
        try {
            const [folder, file] = await locate(name, false);
            await folder.removeEntry(file);
        } catch (error) {
            if (error.name !== "NotFoundError") {
                throw error;
            }
        }
    },
    async read({ name, offset, size }) {
        const handle = await open(name, false);
        const bytes = new Uint8Array(size);
        const read = handle.read(bytes, { at: offset });
        return { bytes: read === size ? bytes : null };
    },
    async estimate() {
        return { available: await available() };
    },
};

// The changes after one that the browser refused would write a state whose records never reached the disk, so the store keeps none of them, and the files stay at the last state they hold whole.
const changes = new Set(["write", "append", "flush", "truncate", "remove"]);
let refused = null;

self.onmessage = (event) => {
    const message = event.data;
    queue = queue.then(async () => {
        if (refused && changes.has(message.op)) {
            self.postMessage({ op: message.op, id: message.id, size: message.size, ok: false, quota: refused.quota, message: refused.message });
            return;
        }
        try {
            const result = (await operations[message.op](message)) || {};
            const transfer = result.bytes ? [result.bytes.buffer] : result.files ? result.files.map((file) => file.bytes.buffer) : [];
            self.postMessage({ op: message.op, id: message.id, size: message.size, ok: true, ...result }, transfer);
        } catch (error) {
            const failure = { quota: error.name === "QuotaExceededError", message: String(error.message || error) };
            if (changes.has(message.op)) {
                refused = failure;
            }
            self.postMessage({ op: message.op, id: message.id, size: message.size, ok: false, ...failure });
        }
    });
};
