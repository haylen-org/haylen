// Engine side of the Haylen web runtime, included before the generated module code. It runs where the WebAssembly module runs: on the page, next to the page side of `haylen-page.js`, or in the dedicated worker of the Worker mode, which `haylen-worker.js` starts and whose page side reaches it through messages. It holds what needs the memory and the exports of the module: the calls into the engine, the editor contract, the WebSockets of the app, the content store, the downloads, the user data, the mixer of the audio outputs, the pictures of videos and the pushes of the streams of plugins.

Module.haylen = Module.haylen || {};

(function (haylen) {
    const hostedInWorker = haylen.hosting === "worker";

    // Reserves wasm memory, which fails with the size instead of handing out the address 0 when the memory cannot grow that far.
    const allocate = (size) => {
        const address = Module._malloc(size);
        if (address === 0) {
            throw new Error("The runtime could not reserve " + size + " bytes of WebAssembly memory.");
        }

        return address;
    };

    // Calls an export of the engine with `ccall`, but with every string argument copied to the heap of the program for the duration of the call, because `ccall` copies strings onto its stack, which a large payload, such as the JSON of a result or the text of a long text area, would overflow.
    const callEngine = (name, returns, types, values) => {
        const owned = [];
        const args = values.map((value, index) => {
            if (types[index] !== "string") {
                return value;
            }

            const size = Module.lengthBytesUTF8(value) + 1;
            const address = allocate(size);
            owned.push(address);
            Module.stringToUTF8(value, address, size);
            return address;
        });
        try {
            return Module.ccall(name, returns, types.map((type) => (type === "string" ? "number" : type)), args);
        } finally {
            owned.forEach((address) => Module._free(address));
        }
    };

    // Copies the buffers into wasm memory with a table of an address and a size for each, for the duration of a call that copies them into the engine.
    const withBuffers = (buffers, body) => {
        const addresses = buffers.map((buffer) => {
            const address = allocate(Math.max(1, buffer.length));
            Module.HEAPU8.set(buffer, address);
            return address;
        });
        const table = allocate(Math.max(1, buffers.length) * 8);
        addresses.forEach((address, index) => {
            Module.HEAPU32[(table >> 2) + index * 2] = address;
            Module.HEAPU32[(table >> 2) + index * 2 + 1] = buffers[index].length;
        });

        try {
            return body(table, buffers.length);
        } finally {
            addresses.forEach((address) => Module._free(address));
            Module._free(table);
        }
    };

    // Copies the buffers of a call out of the table in wasm memory that the engine hands over.
    haylen.readBuffers = function (table, count) {
        const buffers = [];
        for (let index = 0; index < count; ++index) {
            const address = Module.HEAPU32[(table >> 2) + index * 2];
            buffers.push(Module.HEAPU8.slice(address, address + Module.HEAPU32[(table >> 2) + index * 2 + 1]));
        }

        return buffers;
    };

    // Calls an export of the engine, where the type `buffers` stands for a list of byte buffers, which the export receives as the address of a table of an address and a size for each, and their count.
    const call = (name, returns, types, values) => {
        const at = types.indexOf("buffers");
        if (at < 0) {
            return callEngine(name, returns, types, values);
        }

        const expanded = [...types.slice(0, at), "number", "number", ...types.slice(at + 1)];
        return withBuffers(values[at], (table, count) => callEngine(name, returns, expanded, [...values.slice(0, at), table, count, ...values.slice(at + 1)]));
    };

    // Work that needs the WebAssembly runtime waits for it.
    const runtime = { ready: false, waiting: [] };
    const whenReady = (work) => {
        if (runtime.ready) {
            work();
            return;
        }

        runtime.waiting.push(work);
    };

    Module.postRun = Module.postRun || [];
    Module.postRun.push(function () {
        runtime.ready = true;
        for (const work of runtime.waiting.splice(0)) {
            work();
        }
    });

    haylen.persist = function () {
        FS.syncfs(false, (error) => {
            if (error) {
                console.error("Saving user data failed.", error);
            }
        });
    };

    // The editor contract, version 1 of `haylen.editor.protocol`: the requests that an editor sends to the runtime through `haylen.editor.request(method, params)`. Every request gets an id and a promise, which resolves with the result and the id as `request` once the request completed, and rejects with an `Error` whose `code` is a stable code of the contract and whose message is a sentence for the person. The callbacks `onLog`, `onError`, `onStarted`, `onStopped`, `onReloaded` and `onStats` carry what the app does by itself.
    const editor = { protocol: 1, pending: new Map(), nextRequest: 1 };
    haylen.editor = editor;

    // The most files and bytes that the package of a project may hold, which a page sets as `Module.haylen.projectBudget` before the runtime loads, and the edits and zipped packages that would pass either fail with the code `projectBudget`.
    const projectBudget = Object.assign({ files: 4096, bytes: 256 * 1024 * 1024 }, haylen.projectBudget);
    if (![projectBudget.files, projectBudget.bytes].every((value) => Number.isSafeInteger(value) && value > 0) || Object.keys(projectBudget).length !== 2) {
        throw new Error("The field \"Module.haylen.projectBudget\" takes \"files\" and \"bytes\" as whole numbers above 0.");
    }

    haylen.projectBudget = projectBudget;

    const requestError = (code, message) => Object.assign(new Error(message), { code });

    // Requests of the engine that fail answer -1 and keep the error as JSON `{code, message}`.
    const checked = (result) => {
        if (result < 0) {
            const error = JSON.parse(Module.UTF8ToString(Module._haylen_web_last_error()));
            throw requestError(error.code, error.message);
        }

        return result;
    };

    // The answer of a request that waits for the engine, which the runtime sends once the frame that applied it or the start of the app it ran ended.
    haylen.completeRequest = function (request, ok, result) {
        const pending = editor.pending.get(request);
        editor.pending.delete(request);
        if (pending === undefined) {
            return;
        }

        if (!ok) {
            pending.reject(requestError(result.code, result.message));
            return;
        }

        pending.resolve(result);
    };

    // Starts a request whose answer the engine sends later, and forgets it when the start already fails.
    const completeLater = (request, start) => {
        const completion = new Promise((resolve, reject) => editor.pending.set(request, { resolve, reject }));
        try {
            start();
        } catch (error) {
            editor.pending.delete(request);
            throw error;
        }

        return completion;
    };

    // What this runtime offers the editor, which `hello` answers.
    editor.capabilities = function () {
        const editable = !haylen.release;
        return {
            packageEditing: editable,
            developmentReload: editable && haylen.development === true,
            pause: true,
            console: true,
            inspection: true,
            cancellation: haylen.development === true,
            storageNamespaces: true,
            workerHosting: hostedInWorker,
        };
    };

    // A write carries its content as a string, which goes as UTF-8, or as bytes, an `ArrayBuffer` or a view of one.
    const encodeEdits = (operations) => {
        if (!Array.isArray(operations)) {
            throw requestError("invalidRequest", "The edits of a batch are a list of operations.");
        }

        const buffers = [];
        const list = operations.map((operation) => {
            if (operation === null || typeof operation !== "object" || operation.op !== "write") {
                return operation;
            }

            const content = operation.content;
            if (typeof content === "string") {
                buffers.push(new TextEncoder().encode(content));
            } else if (content instanceof ArrayBuffer || ArrayBuffer.isView(content)) {
                buffers.push(content instanceof ArrayBuffer ? new Uint8Array(content) : new Uint8Array(content.buffer, content.byteOffset, content.byteLength));
            } else {
                throw requestError("invalidRequest", "The write of \"" + operation.path + "\" needs its content as a string or as bytes.");
            }

            return { op: "write", path: operation.path, bytes: buffers.length - 1 };
        });
        return { json: JSON.stringify(list), buffers };
    };

    const methods = {
        hello: () => ({ protocol: editor.protocol, capabilities: editor.capabilities() }),

        // Applies a batch of `{op: "write", path, content}`, `{op: "remove", path}` and `{op: "rename", from, to}` as one change of the package, or none of them when one fails, and answers the revision of the package that it made.
        edit: (request, params) => {
            const { json, buffers } = encodeEdits(params.operations);
            const revision = withBuffers(buffers, (table, count) => checked(callEngine("haylen_web_edit", "number", ["string", "number", "number"], [json, table, count])));
            return { revision };
        },

        // Stops the app and empties its package, for an app that the editor builds file by file before `run`.
        clearFiles: () => ({ revision: checked(Module._haylen_web_clear_files()) }),

        // Applies every file that the edits changed since the last `applyChanges`, as one batch at the start of the next frame, and answers once that frame ended, with the revision it applied and the report of hot reload, or with a restart outside development.
        applyChanges: (request) => completeLater(request, () => checked(Module._haylen_web_apply_changes(request))),

        // Starts the package from `source/main.lua` in place of the running app, and answers once it started, with the error that stopped its start, if any.
        run: (request) => completeLater(request, () => checked(Module._haylen_web_run_files(request))),

        // Replaces the package with a zipped one, as bytes, and runs it.
        loadZip: (request, params) => {
            const data = params.bytes instanceof Uint8Array ? params.bytes : new Uint8Array(params.bytes);
            const pointer = allocate(Math.max(1, data.length));
            Module.HEAPU8.set(data, pointer);
            try {
                return completeLater(request, () => checked(Module._haylen_web_load_zip(request, pointer, data.length)));
            } finally {
                Module._free(pointer);
            }
        },

        // Ends the app and leaves an empty canvas until the next run.
        stop: () => ({ revision: checked(Module._haylen_web_stop()) }),

        // A paused app keeps its last frame on screen, sees the pause as a suspend and applies no changes until it resumes.
        pause: () => {
            Module._haylen_web_set_paused(1);
            return { paused: true };
        },
        resume: () => {
            Module._haylen_web_set_paused(0);
            return { paused: false };
        },

        // Runs a line of the developer console in the running app once the next frame runs.
        console: (request, params) => completeLater(request, () => callEngine("haylen_web_console", null, ["number", "string"], [request, String(params.line)])),

        // Ends the runtime for good: the app stops, the player ends with its window and graphics device, the audio contexts close and the user data reaches its database, so the page can discard the frame or the worker that hosts the runtime. Every request after it fails with `disposed`. In the Worker mode the page closes the audio contexts once the worker answered, and ends the worker.
        dispose: () => {
            const ended = new Promise((resolve) => {
                editor.ending = resolve;
            });
            Module._haylen_web_dispose();
            return ended
                .then(() => (hostedInWorker ? undefined : haylen.audio.dispose()))
                .then(() => new Promise((resolve, reject) => FS.syncfs(false, (error) => (error ? reject(requestError("failed", "Saving user data failed: " + error)) : resolve()))))
                .then(() => ({ disposed: true }));
        },

        // Lists the values of the running app at a path of the inspector, such as `scenes.1`, without changing them.
        inspect: (request, params) => completeLater(request, () => callEngine("haylen_web_inspect", null, ["number", "string"], [request, String(params.path || "")])),
    };

    // The player calls it once it ended, after a dispose.
    haylen.reportShutdown = function () {
        editor.disposed = true;
        if (editor.ending) {
            editor.ending();
        }
    };

    editor.request = function (method, params = {}) {
        return new Promise((resolve, reject) => {
            whenReady(() => {
                const request = editor.nextRequest++;
                try {
                    if (editor.disposed || editor.ending) {
                        throw requestError("disposed", "The runtime was disposed of, so it takes no more requests.");
                    }

                    if (!Object.hasOwn(methods, method)) {
                        throw requestError("invalidRequest", "The editor has no request \"" + method + "\".");
                    }

                    Promise.resolve(methods[method](request, params)).then((result) => resolve({ request, ...result }), reject);
                } catch (error) {
                    reject(error);
                }
            });
        });
    };

    // WebSockets the app opened through `haylen.net`, by the id the runtime gave them. Their events reach the app on its next frame.
    const sockets = new Map();

    haylen.openSocket = function (id, url, protocols) {
        const report = (name, types, values) => callEngine(name, null, ["number", ...types], [id, ...values]);
        let socket;
        try {
            socket = new WebSocket(url, protocols);
        } catch (error) {
            // An address the browser rejects fails later, like any connection that fails.
            setTimeout(() => {
                report("haylen_web_socket_failed", ["string"], ["The WebSocket connection to \"" + url + "\" failed. " + String(error.message)]);
                report("haylen_web_socket_closed", ["number", "string"], [1006, ""]);
            });
            return;
        }

        socket.binaryType = "arraybuffer";
        socket.onopen = () => report("haylen_web_socket_opened", ["string"], [socket.protocol]);
        socket.onerror = () => report("haylen_web_socket_failed", ["string"], ["The WebSocket connection to \"" + url + "\" failed."]);
        socket.onclose = (event) => {
            sockets.delete(id);
            report("haylen_web_socket_closed", ["number", "string"], [event.code, event.reason]);
        };
        socket.onmessage = (event) => {
            const binary = typeof event.data !== "string";
            const bytes = binary ? new Uint8Array(event.data) : new TextEncoder().encode(event.data);
            const pointer = allocate(Math.max(1, bytes.length));
            Module.HEAPU8.set(bytes, pointer);

            try {
                report("haylen_web_socket_received", ["number", "number", "number"], [pointer, bytes.length, binary ? 1 : 0]);
            } finally {
                Module._free(pointer);
            }
        };
        sockets.set(id, socket);
    };

    // A socket that closed is gone before the app sees its close event, and what the app sends in between is dropped like the browser drops data sent while closing.
    haylen.sendSocket = function (id, bytes, binary) {
        const socket = sockets.get(id);
        if (socket) {
            socket.send(binary ? bytes : new TextDecoder().decode(bytes));
        }
    };

    // The bytes the browser still holds to send, which a socket that closed or was never kept has none of.
    haylen.socketBufferedAmount = function (id) {
        const socket = sockets.get(id);
        return socket ? socket.bufferedAmount : 0;
    };

    // A socket whose address the browser rejected was never kept, so there is nothing to close.
    haylen.closeSocket = function (id, code, reason) {
        const socket = sockets.get(id);
        if (socket) {
            socket.close(code, reason);
        }
    };

    // Ends a socket the app let go of, without reporting anything more.
    haylen.releaseSocket = function (id) {
        const socket = sockets.get(id);
        if (socket) {
            socket.onopen = socket.onmessage = socket.onerror = socket.onclose = null;
            socket.close();
            sockets.delete(id);
        }
    };

    // The content store of a protected release keeps downloaded content in the private file system of the origin through the worker of `haylen-content-store.js`, so a later visit finds it there. The runtime writes the small files of the store, its state, journal, manifests, shard indexes and staged records, to `/content-store` and hands every change to the worker in order, while segments stay with the worker and come in by range when the runtime asks for them. A browser without the private file system, or a store that another tab of the same app holds, leaves the downloads in memory for the visit.
    haylen.contentStore = (function () {
        let worker = null;
        let space = 0;
        let unwritten = 0;
        let opened = false;
        let refused = false;
        let nextId = 1;
        const replies = new Map();
        const reading = new Set();

        const post = (message, transfer) => {
            if (worker) {
                worker.postMessage(message, transfer || []);
            }
        };
        const ask = (message) =>
            new Promise((resolve) => {
                const id = nextId++;
                replies.set(id, resolve);
                worker.postMessage({ ...message, id });
            });
        const deliver = (id, bytes) => {
            reading.delete(id);
            const size = bytes ? bytes.length : 0;
            const pointer = size > 0 ? allocate(size) : 0;
            if (size > 0) {
                Module.HEAPU8.set(bytes, pointer);
            }

            Module._haylen_web_store_received(id, pointer, size, bytes ? 1 : 0);

            if (pointer) {
                Module._free(pointer);
            }
        };

        // A worker that fails to load or stops on an error ends the store: what waits for it fails, the reads find nothing, and the downloads that follow stay in memory, as the store had refused them.
        const stop = (reason) => {
            worker.terminate();
            worker = null;
            unwritten = 0;

            for (const resolve of replies.values()) {
                resolve({ ok: false, message: reason });
            }

            replies.clear();

            for (const id of [...reading]) {
                deliver(id, null);
            }

            if (opened && !refused) {
                refused = true;
                console.warn("The worker of the content store stopped, so the downloads of this visit stay in memory: " + reason);
                callEngine("haylen_web_store_refused", null, ["number", "string"], [0, reason]);
            }
        };

        return {
            async open(identifier) {
                if (!navigator.storage || !navigator.storage.getDirectory || typeof Worker === "undefined") {
                    console.warn("This browser keeps no files for the page, so downloaded content downloads again on the next visit.");
                    return;
                }

                worker = new Worker(locateFile("haylen-content-store.js"));
                worker.onerror = (event) => {
                    event.preventDefault();
                    stop(event.message || "Its script did not load.");
                };
                worker.onmessage = (event) => {
                    const message = event.data;
                    if (message.op === "read") {
                        deliver(message.id, message.ok ? message.bytes : null);
                        return;
                    }

                    if (replies.has(message.id)) {
                        replies.get(message.id)(message);
                        replies.delete(message.id);
                        return;
                    }

                    if (!message.ok && !refused) {
                        refused = true;
                        console.warn("The browser did not keep a change of the downloaded content, so it keeps the content it held before: " + message.message);
                        callEngine("haylen_web_store_refused", null, ["number", "string"], [message.quota ? 1 : 0, message.message]);
                    }

                    if (message.op === "estimate" && message.ok) {
                        space = message.available;
                    }

                    if (message.op === "append") {
                        unwritten -= message.size;
                    }
                };
                const store = await ask({ op: "open", identifier });
                if (!store.ok) {
                    console.warn("The downloaded content of the app is held by another tab or the browser refused it, so this visit keeps its downloads in memory: " + store.message);
                    if (worker) {
                        worker.terminate();
                        worker = null;
                    }

                    return;
                }

                opened = true;
                FS.mkdirTree("/content-store");

                for (const file of store.files) {
                    const folder = file.name.includes("/") ? "/content-store/" + file.name.slice(0, file.name.lastIndexOf("/")) : "/content-store";
                    FS.mkdirTree(folder);
                    FS.writeFile("/content-store/" + file.name, file.bytes);
                }

                FS.writeFile("/content-store/.segments", JSON.stringify(store.segments));
                space = store.available;

                if (navigator.storage.persist) {
                    navigator.storage.persist().catch(() => {});
                }
            },
            write(name, bytes) {
                post({ op: "write", name, bytes }, [bytes.buffer]);
                post({ op: "estimate" });
            },
            // The room of the origin is estimated again after each whole file, such as every state the store commits, and not after each record a download appends, which would hold the queue of the worker up.
            append(name, bytes) {
                if (worker) {
                    unwritten += bytes.length;
                }

                post({ op: "append", name, bytes, size: bytes.length }, [bytes.buffer]);
            },
            // The bytes of appends that the worker has not written yet, which downloads keep within a bound.
            unwritten() {
                return unwritten;
            },
            // Whether the files of the store live in the browser, or the downloads of the visit stay in memory.
            persistent() {
                return worker !== null;
            },
            flush(name) {
                post({ op: "flush", name });
            },
            truncate(name, size) {
                post({ op: "truncate", name, size });
            },
            remove(name) {
                post({ op: "remove", name });
            },
            read(id, name, offset, size) {
                if (worker) {
                    reading.add(id);
                    worker.postMessage({ op: "read", id, name, offset, size });
                } else {
                    setTimeout(() => deliver(id, null), 0);
                }
            },
            available() {
                return worker ? space : 0;
            },
        };
    })();

    // A page that leaves the WebAssembly module to the runtime, neither instantiating it with `Module.instantiateWasm` nor handing its bytes as `Module.wasmBinary`, such as the default shell of C++ apps, sees the module, the files that the build preloads and the package of `packageUrl` download through `onLoadProgress({loaded, total})`. The `total` is 0 while the size of a download is unknown, which is the case of a compressed response, since it tells only its compressed length.
    const downloads = new Map();
    const reportDownloads = () => {
        let loaded = 0;
        let total = 0;
        let known = true;
        for (const download of [...downloads.values(), ...Object.values(Module.dataFileDownloads || {})]) {
            loaded += download.loaded;
            total += download.total;
            known = known && download.total > 0;
        }

        haylen.reportLoadProgress({ loaded, total: known ? total : 0 });
    };

    const track = (url, response) => {
        const length = Number(response.headers.get("Content-Length"));
        const download = { loaded: 0, total: !response.headers.get("Content-Encoding") && length > 0 ? length : 0 };
        downloads.set(url, download);
        const reader = response.body.getReader();
        const body = new ReadableStream({
            async pull(controller) {
                const { done, value } = await reader.read();

                if (done) {
                    controller.close();
                    return;
                }

                download.loaded += value.length;
                reportDownloads();
                controller.enqueue(value);
            },
        });
        return new Response(body, { headers: response.headers });
    };

    // Emscripten tells about every chunk of the files that the build preloads through `Module.setStatus`, with the sizes in `Module.dataFileDownloads`, and a status function of the page still hears it.
    const pageStatus = Module.setStatus;
    Module.setStatus = function (text) {
        reportDownloads();

        if (pageStatus) {
            pageStatus(text);
        }
    };

    if (!Module.instantiateWasm && !Module.wasmBinary) {
        Module.instantiateWasm = function (imports, receive) {
            const url = findWasmBinary();
            fetch(url)
                .then((response) => {
                    if (!response.ok) {
                        throw new Error(response.status + " " + response.statusText);
                    }

                    return track(url, response).arrayBuffer();
                })
                .then((bytes) => WebAssembly.instantiate(bytes, imports))
                .then((result) => receive(result.instance))
                .catch((error) => {
                    haylen.reportError({ message: "The WebAssembly module \"" + url + "\" could not be loaded, and the browser reported \"" + error.message + "\".", file: "", line: 0, traceback: "" });
                });
        };
    }

    // User data lives in IndexedDB under `/persistent`, loaded before the app starts so saves are there on the first frame. A page that names `Module.haylen.storageNamespace`, such as an editor that plays many projects, keeps the user data of each namespace in a database of its own, named after its folder `/persistent/<namespace>`, so projects never see each other's data and a page loads only the namespace it plays.
    const storageNamespace = haylen.storageNamespace;
    if (storageNamespace !== undefined && (typeof storageNamespace !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$/.test(storageNamespace))) {
        throw new Error("The field \"Module.haylen.storageNamespace\" must be 1 to 64 letters, digits, dots, dashes or underscores, starting with a letter or a digit.");
    }

    haylen.userDataRoot = storageNamespace === undefined ? "/persistent" : "/persistent/" + storageNamespace;
    Module.preRun = Module.preRun || [];
    Module.preRun.push(function () {
        FS.mkdirTree(haylen.userDataRoot);
        FS.mount(IDBFS, {}, haylen.userDataRoot);
        addRunDependency("haylen-user-data");
        FS.syncfs(true, (error) => {
            if (error) {
                console.error("Loading user data failed.", error);
            }

            removeRunDependency("haylen-user-data");
        });
    });

    // A protected release reaches the runtime as `Module.haylen.release`: the manifests and the app domain that the page downloaded, which go to `/app`, the address of the release, where the shards of its content stay and download by range, and the identifier of the app, which names its content store.
    if (haylen.release) {
        haylen.releaseAddress = hostedInWorker ? haylen.release.address : new URL(haylen.release.address, document.baseURI).href;
        Module.preRun.push(function () {
            FS.mkdirTree("/app");

            for (const file of haylen.release.files) {
                FS.writeFile("/app/" + file.name, new Uint8Array(file.bytes));
            }

            haylen.release.files = null;
            addRunDependency("haylen-content-store");
            haylen.contentStore.open(haylen.release.identifier).finally(() => removeRunDependency("haylen-content-store"));
        });
    }

    // A page that already holds the zipped package, such as a loader that downloaded it with a progress bar, hands its bytes over as `Module.haylen.packageData`, and a page can instead name a zipped package with `Module.haylen.packageUrl`, which is downloaded before the app starts. The runtime plays it instead of the bundled package. Only a page that sets `Module.haylen.development` runs the app in development, connected to the development server that `Module.haylen.developmentServer` names, if any. The graphics API that `Module.haylen.graphicsApi` names, `webGpu` or `webGl2`, is the one the window tries first, and the list of strings `Module.haylen.arguments` holds the launch arguments of the app.
    const launch = [];
    if (haylen.packageData || haylen.packageUrl) {
        launch.push("/package.zip");
    }

    if (haylen.graphicsApi) {
        launch.push("--graphics-api", haylen.graphicsApi);
    }

    if (haylen.development) {
        launch.push("--dev");
    }

    if (haylen.developmentServer) {
        launch.push("--dev-server", haylen.developmentServer);
    }

    if (haylen.arguments && haylen.arguments.length > 0) {
        launch.push("--", ...haylen.arguments);
    }

    if (launch.length > 0) {
        Module.arguments = launch;
    }

    if (haylen.packageData) {
        Module.preRun.push(function () {
            FS.writeFile("/package.zip", new Uint8Array(haylen.packageData));
            haylen.packageData = null;
        });
    } else if (haylen.packageUrl) {
        Module.preRun.push(function () {
            addRunDependency("haylen-package");
            fetch(haylen.packageUrl)
                .then((response) => {
                    if (!response.ok) {
                        throw new Error(response.status + " " + response.statusText);
                    }

                    return track(haylen.packageUrl, response).arrayBuffer();
                })
                .then((buffer) => FS.writeFile("/package.zip", new Uint8Array(buffer)))
                .catch((error) => {
                    const message = "The package \"" + haylen.packageUrl + "\" could not be downloaded, and the browser reported \"" + error.message + "\".";
                    console.error(message);
                    haylen.reportError({ message: message, file: "", line: 0, traceback: "" });
                })
                .finally(() => removeRunDependency("haylen-package"));
        });
    }

    // The calls of the engine into the page: the functions of the page side on `Module.haylen` on the page, and in the Worker mode the functions that post each call to the page, which `haylen-worker.js` also lists.
    const page = hostedInWorker ? {} : haylen;

    // The mixer of the audio outputs that `BrowserAudioOutput.cpp` drives, one per audio device of the engine, which mixes blocks of interleaved samples and posts them to the processor of the output through the port that the page hands it. The processor answers with the frames it played and the buffers of the blocks it copied, and the mixer mixes new blocks until the audio queued ahead of the output is back at its target, so neither shared memory nor threads are needed. The page side owns the audio context and the node of each output. A context or a processor that the browser refuses reaches the engine through `haylen_web_audio_failed`, which leaves the app without sound with the reason in its log, on the page and in the Worker mode alike.
    const mixer = { outputs: new Map() };
    haylen.mixer = mixer;

    mixer.supported = function () {
        return hostedInWorker ? haylen.state.audioSupported : haylen.audio.supported();
    };

    mixer.open = function (device, channels, sampleRate, blockFrames, bufferedFrames) {
        mixer.outputs.set(device, { device, channels, sampleRate, blockFrames, bufferedFrames, port: null, started: false, queued: 0, blocks: 0, underruns: 0, spare: [], scratch: allocate(blockFrames * channels * Float32Array.BYTES_PER_ELEMENT) });
        page.audio.open(device, channels, sampleRate, blockFrames, bufferedFrames);
    };

    const mixBlock = (output) => {
        const samples = output.spare.pop() || new Float32Array(output.blockFrames * output.channels);
        Module._haylen_web_audio_render(output.device, output.scratch, output.blockFrames);
        const first = output.scratch / Float32Array.BYTES_PER_ELEMENT;
        samples.set(Module.HEAPF32.subarray(first, first + samples.length));
        output.port.postMessage(samples, [samples.buffer]);
        output.queued += output.blockFrames;
        output.blocks += 1;
    };

    const fillAudio = (output) => {
        while (output.started && output.port !== null && output.queued + output.blockFrames <= output.bufferedFrames) {
            mixBlock(output);
        }
    };

    // Takes the port of the processor once the page connected it, unless the output closed meanwhile.
    mixer.connect = function (device, port) {
        const output = mixer.outputs.get(device);
        if (output === undefined) {
            port.close();
            return;
        }

        output.port = port;
        port.onmessage = (event) => {
            output.queued -= event.data.played;
            output.underruns = event.data.underruns;
            for (const buffer of event.data.buffers) {
                output.spare.push(new Float32Array(buffer));
            }

            fillAudio(output);
        };

        if (output.started) {
            port.postMessage({ active: true });
            fillAudio(output);
        }
    };

    // The engine starts and stops the output from inside a frame, so the first blocks are mixed once the frame returned.
    mixer.start = function (device) {
        const output = mixer.outputs.get(device);
        output.started = true;
        if (output.port !== null) {
            output.port.postMessage({ active: true });
        }

        page.audio.start(device);
        queueMicrotask(() => fillAudio(output));
    };

    // The processor keeps what it has queued and plays silence, so the output goes on where it stopped.
    mixer.stop = function (device) {
        const output = mixer.outputs.get(device);
        output.started = false;
        if (output.port !== null) {
            output.port.postMessage({ active: false });
        }

        page.audio.stop(device);
    };

    // The processor of a closed output ends, so the browser releases its node.
    mixer.close = function (device) {
        const output = mixer.outputs.get(device);
        mixer.outputs.delete(device);
        if (output.port !== null) {
            output.port.onmessage = null;
            output.port.postMessage({ ended: true });
            output.port.close();
        }

        Module._free(output.scratch);
        page.audio.close(device);
    };

    mixer.locked = function (device) {
        return hostedInWorker ? haylen.state.audioLocks.includes(device) : haylen.audio.locked(device);
    };

    // What the mixer tells about the first output for `onStats`.
    mixer.stats = function () {
        const [output] = mixer.outputs.values();
        if (!output) {
            return {};
        }

        return { bufferedMilliseconds: (output.queued * 1000) / output.sampleRate, blocks: output.blocks, underruns: output.underruns };
    };

    // The videos of the engine that `WebVideoDecoder.cpp` reads, which play in elements of the page side. On the page the engine reads their pictures and sound from the elements, and in the Worker mode from what the page posts: the newest picture of each video as an `ImageBitmap`, which the worker draws into a canvas of the size the engine asks for, what the element knows and its sound.
    const videos = { nextId: 1, parts: new Map(), posted: new Map() };

    const describeVideo = () => ({ width: 0, height: 0, duration: 0, sampleRate: 0, error: "", ended: false, time: 0 });

    // The answer is 0 while no new picture shows, 1 for a picture, 2 at the end and 3 for an error.
    const readPostedFrame = (id, pixels, width, height) => {
        const video = videos.posted.get(id);
        if (video.state.error) {
            return 3;
        }

        if (video.bitmap === null) {
            return video.state.ended ? 2 : 0;
        }

        if (width <= 0 || height <= 0) {
            return 0;
        }

        if (!video.canvas || video.canvas.width !== width || video.canvas.height !== height) {
            video.canvas = new OffscreenCanvas(width, height);
            video.context = video.canvas.getContext("2d", { willReadFrequently: true });
        }

        video.context.drawImage(video.bitmap, 0, 0, width, height);
        pixels.set(video.context.getImageData(0, 0, width, height).data);
        video.bitmap.close();
        video.bitmap = null;
        return 1;
    };

    const takePostedSound = (id, samples) => {
        const video = videos.posted.get(id);
        let copied = 0;
        while (video.samples.length > 0 && copied < samples.length) {
            const block = video.samples[0];
            const count = Math.min(block.length, samples.length - copied);
            samples.set(block.subarray(0, count), copied);
            copied += count;

            if (count === block.length) {
                video.samples.shift();
            } else {
                video.samples[0] = block.subarray(count);
            }
        }

        return copied;
    };

    haylen.videoDecoder = {
        begin() {
            const id = videos.nextId++;
            videos.parts.set(id, []);
            return id;
        },
        append(id, pointer, length) {
            videos.parts.get(id).push(Module.HEAPU8.slice(pointer, pointer + length));
        },
        open(id, type) {
            const parts = videos.parts.get(id);
            videos.parts.delete(id);

            if (hostedInWorker) {
                videos.posted.set(id, { state: describeVideo(), bitmap: null, canvas: null, context: null, samples: [] });
            }

            page.video.open(id, type, parts);
        },
        info(id) {
            return JSON.stringify(hostedInWorker ? videos.posted.get(id).state : haylen.video.info(id));
        },
        control(id, playing, rate) {
            page.video.control(id, playing, rate);
        },
        frame(id, pointer, width, height) {
            const pixels = Module.HEAPU8.subarray(pointer, pointer + width * height * 4);
            return hostedInWorker ? readPostedFrame(id, pixels, width, height) : haylen.video.frame(id, pixels, width, height);
        },
        takeSound(id, pointer, capacity) {
            const first = pointer / Float32Array.BYTES_PER_ELEMENT;
            const samples = Module.HEAPF32.subarray(first, first + capacity);
            return hostedInWorker ? takePostedSound(id, samples) : haylen.video.takeSound(id, samples);
        },
        seek(id, seconds) {
            if (hostedInWorker) {
                const video = videos.posted.get(id);
                video.samples = [];
                if (video.bitmap !== null) {
                    video.bitmap.close();
                    video.bitmap = null;
                }
            }

            page.video.seek(id, seconds);
        },
        time(id) {
            return hostedInWorker ? videos.posted.get(id).state.time : haylen.video.time(id);
        },
        close(id) {
            const video = videos.posted.get(id);
            if (video !== undefined && video.bitmap !== null) {
                video.bitmap.close();
            }

            videos.posted.delete(id);
            page.video.close(id);
        },
    };

    // The streams of the web parts of plugins by the key of the page side, each with its handle in the engine and a block of wasm memory for its pushes.
    const streams = new Map();

    // Keeps a block of wasm memory of at least `size` bytes for the pushes of a stream. A new block may grow the memory, which detaches the views taken before, so callers read `Module.HEAPU8` or `Module.HEAPF32` only after it.
    const reserveMemory = (stream, size) => {
        if (stream.size < size) {
            const memory = allocate(size);
            Module._free(stream.memory);
            stream.memory = memory;
            stream.size = size;
        }

        return stream.memory;
    };

    const roomOf = (stream) => (stream.kind === "audio" ? Module._haylen_web_audio_stream_room(stream.handle) : 0);

    const openStream = (key, kind, args) => {
        const handle = kind === "video" ? callEngine("haylen_web_open_video_stream", "number", ["string", "string"], args) : callEngine("haylen_web_open_audio_stream", "number", ["string", "string", "number", "number", "number"], args);
        if (!handle) {
            console.error(Module.UTF8ToString(Module._haylen_web_last_error()));
            page.streamOpened(key, false, 0);
            return;
        }

        const stream = { handle, kind, memory: 0, size: 0 };
        streams.set(key, stream);
        page.streamOpened(key, true, roomOf(stream));
    };

    const pushVideo = (key, pixels, width, height, time) => {
        const stream = streams.get(key);
        const memory = reserveMemory(stream, pixels.length);
        Module.HEAPU8.set(pixels, memory);
        return checked(Module._haylen_web_push_video_frame(stream.handle, memory, width, height, time)) === 1;
    };

    const pushAudio = (key, samples, channels) => {
        const stream = streams.get(key);
        const memory = reserveMemory(stream, samples.length * Float32Array.BYTES_PER_ELEMENT);
        Module.HEAPF32.set(samples, memory / Float32Array.BYTES_PER_ELEMENT);
        return checked(Module._haylen_web_push_audio_frames(stream.handle, memory, samples.length / channels));
    };

    // What the page side asks of the engine side on the page. In the Worker mode the page side makes its own link, whose messages the worker answers with the same functions.
    haylen.engine = {
        call,
        paused: () => Module._haylen_web_paused() === 1,
        connectAudio: (device, port) => mixer.connect(device, port),
        mixerStats: () => mixer.stats(),
        openStream,
        pushVideo,
        pushAudio: (stream, samples, channels) => pushAudio(stream.key, samples, channels),
    };

    // The Worker mode: the engine side answers the page through the messages of the worker. The page posts the calls of its page side into the engine, its input, the state that the engine reads during a frame, the requests of the editor, the ports of the audio outputs, the pushes of streams, the pictures, state and sound of videos and the canvases of outputs. The engine side posts the calls of the engine into the page, the answers of the editor, `ready` once the runtime runs and a heartbeat while the worker answers.
    if (hostedInWorker) {
        const kHeartbeatMilliseconds = 250;
        const kStreamRoomMilliseconds = 50;
        const outputCanvases = new Map();

        // The buffers of the values of a call, which move to the page, while the memory of the module stays.
        const transfers = (values) => {
            const found = [];
            const collect = (value) => {
                if (ArrayBuffer.isView(value) && value.buffer !== Module.HEAPU8.buffer) {
                    found.push(value.buffer);
                } else if (Array.isArray(value)) {
                    value.forEach(collect);
                }
            };
            values.forEach(collect);
            return found;
        };

        const post = (message, transfer) => self.postMessage(message, transfer || []);
        const callPage = (path) => (...values) => post({ type: "page", path, values }, transfers(values));

        // The functions of the page side that the engine calls, which reach the page in the order of the calls.
        const pageCalls = [
            "reportLog",
            "reportError",
            "reportStarted",
            "reportStopped",
            "reportSplashEnded",
            "reportReloaded",
            "reportLoadProgress",
            "setDrawableWidth",
            "dispatch",
            "cancel",
            "openScreen",
            "cancelScreen",
            "setFullscreen",
            "keepScreenOn",
            "awaitPointerLock",
            "lockPointer",
            "writeClipboard",
            "readClipboard",
            "requestEveryScreen",
            "lockOrientation",
            "declareOrientation",
            "openOutput",
            "setTitle",
            "setCursor",
            "rumbleGamepad",
            "streamOpened",
            "streamRoom",
        ];
        for (const path of pageCalls) {
            haylen[path] = callPage(path);
            page[path] = haylen[path];
        }

        haylen.accessibility = { update: callPage("accessibility.update"), announce: callPage("accessibility.announce") };
        haylen.textInput = { edit: callPage("textInput.edit"), finish: callPage("textInput.finish"), setFields: callPage("textInput.setFields") };
        page.audio = { open: callPage("audio.open"), start: callPage("audio.start"), stop: callPage("audio.stop"), close: callPage("audio.close") };
        page.video = { open: callPage("video.open"), control: callPage("video.control"), seek: callPage("video.seek"), close: callPage("video.close") };

        // The statistics carry the numbers of the mixer, which the page merges with what its audio context tells.
        haylen.reportStats = (stats) => callPage("reportStats")({ ...stats, mixer: mixer.stats() });

        // The canvas of an output leaves with the output.
        haylen.closeOutput = (number) => {
            outputCanvases.delete(number);
            callPage("closeOutput")(number);
        };

        // What the engine reads during a frame comes from the state that the page posted last.
        const state = haylen.state;
        haylen.canvasBox = () => state.canvasBox;
        haylen.safeAreaInsets = () => state.safeArea;
        haylen.fold = () => state.fold;
        haylen.orientation = () => state.orientation;
        haylen.isFullscreen = () => state.fullscreen;
        haylen.clipboardText = () => state.clipboard;
        haylen.displays = () => state.displays;
        haylen.languages = () => state.languages;
        haylen.systemInfo = () => state.systemInfo;
        haylen.nativePlugins = () => state.nativePlugins;
        haylen.outputCanvas = (number) => outputCanvases.get(number) ?? null;
        Object.defineProperty(haylen, "gamepads", { get: () => state.gamepads });

        // Reports the room of every audio stream while one is open, so the page knows how many frames fit.
        let roomTimer = null;
        const reportRooms = () => {
            for (const [key, stream] of streams) {
                if (stream.kind === "audio") {
                    page.streamRoom(key, roomOf(stream));
                }
            }
        };

        const receiveStream = (message) => {
            if (message.operation === "open") {
                openStream(message.key, message.kind, message.args);
                if (message.kind === "audio" && roomTimer === null) {
                    roomTimer = setInterval(reportRooms, kStreamRoomMilliseconds);
                }

                return;
            }

            if (!streams.has(message.key)) {
                return;
            }

            if (message.operation === "video") {
                pushVideo(message.key, message.pixels, message.width, message.height, message.time);
                return;
            }

            pushAudio(message.key, message.samples, message.channels);
            page.streamRoom(message.key, roomOf(streams.get(message.key)));
        };

        const receiveVideo = (message) => {
            const video = videos.posted.get(message.id);
            if (video === undefined) {
                if (message.bitmap) {
                    message.bitmap.close();
                }

                return;
            }

            if (message.type === "videoState") {
                video.state = message.state;
            } else if (message.type === "videoSound") {
                video.samples.push(message.samples);
            } else {
                if (video.bitmap !== null) {
                    video.bitmap.close();
                }

                video.bitmap = message.bitmap;
            }
        };

        // Answers a request of the editor with its result, or with the code and the sentence of its failure.
        const answerRequest = (message) => {
            editor.request(message.method, message.params).then(
                (result) => post({ type: "answer", id: message.id, ok: true, result }),
                (error) => post({ type: "answer", id: message.id, ok: false, error: { code: error.code || "failed", message: String(error.message || error) } })
            );
        };

        self.onmessage = (event) => {
            const message = event.data;
            switch (message.type) {
            case "engine":
                whenReady(() => call(message.name, null, message.types, message.values));
                return;
            case "state":
                Object.assign(state, message.values);
                return;
            case "request":
                answerRequest(message);
                return;
            case "audioPort":
                mixer.connect(message.device, message.port);
                return;
            case "stream":
                whenReady(() => receiveStream(message));
                return;
            case "videoFrame":
            case "videoState":
            case "videoSound":
                receiveVideo(message);
                return;
            case "output":
                outputCanvases.set(message.number, message.canvas);
                return;
            default:
                throw new Error("The page sent the worker of the runtime the unknown message \"" + message.type + "\".");
            }
        };

        Module.postRun.push(() => post({ type: "ready" }));
        setInterval(() => post({ type: "heartbeat" }), kHeartbeatMilliseconds);
    }

})(Module.haylen);
