// Page side of the Haylen web runtime, included before the generated module code.
// Pages talk to the running app through Module.haylen: native bridge handlers, the contexts of plugin modules with their overlay over the canvas, the editor API that swaps the app package without reloading the page, and the onLog, onError, onStarted, onStopped and onStats callbacks.

Module.haylen = Module.haylen || {};

(function (haylen) {
    const handlers = new Map();
    const levels = ["debug", "info", "warning", "error"];

    // The system services of the engine: the language of the browser, a url opened in a new tab and the vibration of phones whose browser offers it.
    haylen.locale = function () {
        return navigator.language;
    };

    haylen.openUrl = function (url) {
        const opened = window.open(url, "_blank");
        if (!opened) {
            return false;
        }
        opened.opener = null;
        return true;
    };

    haylen.vibrate = function (seconds) {
        if (navigator.vibrate) {
            navigator.vibrate(Math.max(1, Math.round(seconds * 1000)));
        }
    };

    // Registers an async handler for a platform method. It receives the parsed params and a context with the id of the call and a signal that aborts when the app cancels the call or its timeout passes, and it returns any JSON value. A thrown error fails the call with its message and its code and data, or with the code exception when it has no code.
    haylen.register = function (method, handler) {
        handlers.set(method, handler);
    };

    haylen.unregister = function (method) {
        handlers.delete(method);
    };

    // Work that needs the WebAssembly runtime waits for it, and events wait for the first app, so plugins may call in while they load before the runtime starts.
    const waiting = { runtime: [], events: [], runtimeReady: false, appStarted: false };

    const whenRuntimeReady = (work) => {
        if (waiting.runtimeReady) {
            work();
            return;
        }
        waiting.runtime.push(work);
    };

    // Sends an event to the app, which receives it through haylen.platform.on. An event that nothing listens to is dropped, unless options.retain is true: then it waits for the first listener of its name. Events sent before the first app started reach it once it starts.
    haylen.emit = function (event, payload, options) {
        const values = [event, JSON.stringify(payload === undefined ? null : payload), options && options.retain ? 1 : 0];
        if (!waiting.appStarted) {
            waiting.events.push(values);
            return;
        }
        Module.ccall("haylen_web_emit", null, ["string", "string", "number"], values);
    };

    // The abort controllers of the calls that wait for their handler, so a cancel reaches the handler and its late answer is dropped.
    const pending = new Map();

    // An error with a code keeps its code and data, and any other error fails with the code exception and its name in data.type, as on the other platforms.
    const describeFailure = function (error) {
        const message = String(error && error.message ? error.message : error);
        if (error && error.code !== undefined) {
            return { message, code: error.code, data: error.data };
        }
        return { message, code: "exception", data: { type: error && error.name ? error.name : typeof error } };
    };

    haylen.dispatch = function (call, method, params) {
        const reply = (ok, value) => {
            if (pending.delete(call)) {
                Module.ccall("haylen_web_resolve", null, ["number", "number", "string"], [call, ok ? 1 : 0, JSON.stringify(value === undefined ? null : value)]);
            }
        };
        const handler = handlers.get(method);
        const controller = new AbortController();
        pending.set(call, controller);
        if (!handler) {
            reply(false, { message: "No page handler is registered for " + method + ".", code: "noHandler" });
            return;
        }
        // The handler starts after the frame that made the call, and a cancel in that frame keeps it from starting, as on the other platforms.
        queueMicrotask(async () => {
            if (controller.signal.aborted) {
                return;
            }
            try {
                reply(true, await handler(JSON.parse(params), { call, signal: controller.signal }));
            } catch (error) {
                reply(false, describeFailure(error));
            }
        });
    };

    haylen.cancel = function (call) {
        const controller = pending.get(call);
        if (controller) {
            pending.delete(call);
            controller.abort();
        }
    };

    // The web parts of plugins by id, each with the listeners of its context that hear app errors. Their ids are the plugins whose native part runs on the web.
    const plugins = new Map();

    haylen.nativePlugins = function () {
        return [...plugins.keys()];
    };

    // Native UI such as a full screen ad covers the app, which the engine then halts and mutes until the last cover ends.
    const coverApp = () => whenRuntimeReady(() => Module._haylen_web_cover_app());
    const uncoverApp = () => whenRuntimeReady(() => Module._haylen_web_uncover_app());

    // The overlay layer lies over the canvas and lets the pointer through everywhere except on the elements of plugins, which it places by the anchors of their placements. An element with reserve set reserves the edge it sits on, in canvas pixels, and the engine widens the safe area of the app by it.
    const overlay = { layer: null, resizes: null, items: new Set(), serial: 0 };
    const anchors = {
        top: [0.5, 0],
        bottom: [0.5, 1],
        left: [0, 0.5],
        right: [1, 0.5],
        topLeft: [0, 0],
        topRight: [1, 0],
        bottomLeft: [0, 1],
        bottomRight: [1, 1],
        center: [0.5, 0.5],
    };

    const readPlacement = (placement) => {
        const value = { anchor: "bottom", margin: 0, insideSafeArea: true, reserve: false, width: null, height: null, ...placement };
        if (!(value.anchor in anchors)) {
            throw new Error("The overlay anchor " + value.anchor + " is unknown. It is one of " + Object.keys(anchors).join(", ") + ".");
        }
        return value;
    };

    // The safe area insets of the page in page pixels, as far as they reach into the canvas box.
    const safeInsets = (box) => {
        const [left, top, right, bottom] = haylen.safeAreaInsets().map((value) => value / (window.devicePixelRatio || 1));
        return { left: Math.max(0, left - box.left), top: Math.max(0, top - box.top), right: Math.max(0, right - (window.innerWidth - box.right)), bottom: Math.max(0, bottom - (window.innerHeight - box.bottom)) };
    };

    // Tells the engine only what changed, and insets of null release the edge of the item.
    const reserve = (item, insets) => {
        const previous = item.reserved;
        if (insets === previous || (insets && previous && ["left", "top", "right", "bottom"].every((edge) => insets[edge] === previous[edge]))) {
            return;
        }
        item.reserved = insets;
        const key = item.key;
        if (insets) {
            whenRuntimeReady(() => Module.ccall("haylen_web_reserve_insets", null, ["string", "number", "number", "number", "number"], [key, insets.left, insets.top, insets.right, insets.bottom]));
        } else {
            whenRuntimeReady(() => Module.ccall("haylen_web_release_insets", null, ["string"], [key]));
        }
    };

    // The element keeps its own styles, apart from its position, and a size or a visibility that the placement sets over its own.
    const place = (item, box) => {
        const element = item.element;
        const placement = item.placement;
        element.style.visibility = item.visible ? item.original.visibility : "hidden";
        element.style.width = placement.width === null ? item.original.width : placement.width + "px";
        element.style.height = placement.height === null ? item.original.height : placement.height + "px";
        if (!item.visible) {
            reserve(item, null);
            return;
        }

        const insets = placement.insideSafeArea ? safeInsets(box) : { left: 0, top: 0, right: 0, bottom: 0 };
        const width = element.offsetWidth;
        const height = element.offsetHeight;
        const [horizontal, vertical] = anchors[placement.anchor];
        const margin = placement.margin;
        const areaWidth = box.width - insets.left - insets.right;
        const areaHeight = box.height - insets.top - insets.bottom;
        const x = insets.left + (horizontal === 0.5 ? (areaWidth - width) / 2 : horizontal === 0 ? margin : areaWidth - width - margin);
        const y = insets.top + (vertical === 0.5 ? (areaHeight - height) / 2 : vertical === 0 ? margin : areaHeight - height - margin);
        element.style.left = x + "px";
        element.style.top = y + "px";
        if (!placement.reserve || placement.anchor === "center") {
            reserve(item, null);
            return;
        }

        // The element reserves the edge its anchor names, from the edge of the canvas to its far side, in canvas pixels.
        const scale = box.width > 0 ? Module.canvas.width / box.width : 1;
        const reserved = { left: 0, top: 0, right: 0, bottom: 0 };
        if (vertical === 0) {
            reserved.top = (y + height) * scale;
        } else if (vertical === 1) {
            reserved.bottom = (box.height - y) * scale;
        } else if (horizontal === 0) {
            reserved.left = (x + width) * scale;
        } else {
            reserved.right = (box.width - x) * scale;
        }
        reserve(item, reserved);
    };

    const layout = () => {
        const layer = overlay.layer;
        if (!layer) {
            return;
        }
        const box = Module.canvas.getBoundingClientRect();
        layer.style.left = box.left + "px";
        layer.style.top = box.top + "px";
        layer.style.width = box.width + "px";
        layer.style.height = box.height + "px";
        for (const item of overlay.items) {
            place(item, box);
        }
    };

    const ensureLayer = () => {
        if (overlay.layer) {
            return overlay.layer;
        }
        const layer = document.createElement("div");
        layer.id = "haylen-overlay";
        layer.style.cssText = "position:fixed;left:0;top:0;width:0;height:0;overflow:hidden;pointer-events:none;z-index:2;";
        Module.canvas.after(layer);
        overlay.layer = layer;
        overlay.resizes = new ResizeObserver(layout);
        overlay.resizes.observe(Module.canvas);
        window.addEventListener("resize", layout);
        window.addEventListener("scroll", layout, true);
        if (window.visualViewport) {
            window.visualViewport.addEventListener("resize", layout);
        }
        return layer;
    };

    const addToOverlay = (id, element, placement) => {
        if (!(element instanceof HTMLElement)) {
            throw new Error("The plugin " + id + " can only place an HTML element over the app.");
        }
        const original = { width: element.style.width, height: element.style.height, visibility: element.style.visibility };
        const item = { key: id + "#" + ++overlay.serial, element, original, placement: readPlacement(placement), visible: true, reserved: null };
        element.style.position = "absolute";
        element.style.pointerEvents = "auto";
        element.style.boxSizing = "border-box";
        ensureLayer().appendChild(element);
        overlay.items.add(item);
        overlay.resizes.observe(element);
        layout();
        return {
            update(value) {
                item.placement = readPlacement(value);
                layout();
            },
            setVisible(visible) {
                item.visible = Boolean(visible);
                layout();
            },
            remove() {
                if (!overlay.items.delete(item)) {
                    return;
                }
                overlay.resizes.unobserve(element);
                element.remove();
                reserve(item, null);
            },
        };
    };

    // Makes the context that the web module of a plugin receives in load(context). Methods and events take the id of the plugin in front of their names, as the Lua handle of the plugin expects. The loader calls it for every plugin before the runtime starts.
    haylen.createPluginContext = function (id, config) {
        if (plugins.has(id)) {
            throw new Error("The plugin " + id + " already has a context.");
        }
        const plugin = { errorListeners: [] };
        plugins.set(id, plugin);
        return {
            id,
            config: config || {},
            register(method, handler) {
                haylen.register(id + "." + method, handler);
            },
            emit(event, payload, options) {
                haylen.emit(id + "." + event, payload, options);
            },
            overlay: {
                add: (element, placement) => addToOverlay(id, element, placement),
            },
            coverApp,
            uncoverApp,
            onAppError(listener) {
                plugin.errorListeners.push(listener);
            },
        };
    };

    // The runtime reports from inside a frame, so page callbacks run right after it and may call back into the runtime, even to restart the app.
    const notify = function (name, ...values) {
        const callback = haylen[name];
        if (typeof callback === "function") {
            queueMicrotask(() => callback(...values));
        }
    };

    // Varn also prints every line to stdout, which already reaches the browser console, so pages only get the structured copy here.
    haylen.reportLog = function (level, message) {
        notify("onLog", levels[level] || "info", message);
    };

    // Errors arrive as {message, file, line, traceback, frames}, with an empty file when no script position is known. The page and every plugin that listens with onAppError receive them.
    haylen.reportError = function (error) {
        notify("onError", error);
        for (const plugin of plugins.values()) {
            for (const listener of plugin.errorListeners) {
                queueMicrotask(() => listener(error));
            }
        }
    };

    haylen.reportStarted = function (app) {
        if (!waiting.appStarted) {
            waiting.appStarted = true;
            queueMicrotask(() => {
                for (const values of waiting.events.splice(0)) {
                    Module.ccall("haylen_web_emit", null, ["string", "string", "number"], values);
                }
            });
        }
        notify("onStarted", app);
    };

    haylen.reportStopped = function () {
        notify("onStopped");
    };

    // Statistics arrive about once per second while the app runs: fps, frame times, renderer counters, cached assets, audio voices, profiler scopes and the audio output.
    haylen.reportStats = function (stats) {
        stats.audio = audio.stats();
        notify("onStats", stats);
    };

    haylen.canvasSelector = function () {
        if (!(Module.canvas instanceof HTMLCanvasElement) || !Module.canvas.id) {
            throw new Error("Module.canvas must be a canvas element with an id.");
        }
        return "#" + CSS.escape(Module.canvas.id);
    };

    // Reads env(safe-area-inset-*) through a hidden probe and returns the insets in framebuffer pixels.
    haylen.safeAreaInsets = function () {
        let probe = document.getElementById("haylen-safe-area");
        if (!probe) {
            probe = document.createElement("div");
            probe.id = "haylen-safe-area";
            probe.style.cssText = "position:fixed;visibility:hidden;pointer-events:none;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);";
            document.body.appendChild(probe);
        }
        const style = getComputedStyle(probe);
        const ratio = window.devicePixelRatio || 1;
        return [style.paddingLeft, style.paddingTop, style.paddingRight, style.paddingBottom].map((value) => (parseFloat(value) || 0) * ratio);
    };

    haylen.persist = function () {
        FS.syncfs(false, (error) => {
            if (error) {
                console.error("Saving user data failed", error);
            }
        });
    };

    // Replaces the running app with a zipped package.
    haylen.loadZip = function (bytes) {
        const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
        const pointer = Module._malloc(data.length);
        Module.HEAPU8.set(data, pointer);
        try {
            Module._haylen_web_load_zip(pointer, data.length);
        } finally {
            Module._free(pointer);
        }
    };

    // Editor API: build a package file by file, then run it. The running app reads edited files the next time it loads them, and run starts it again from source/main.lua.
    haylen.clearFiles = function () {
        Module._haylen_web_clear_files();
    };

    // Entry points that can fail answer -1 and keep the reason, which becomes a JavaScript error.
    const checked = (result) => {
        if (result < 0) {
            throw new Error(Module.UTF8ToString(Module._haylen_web_last_error()));
        }
        return result;
    };

    haylen.setFile = function (path, content) {
        const data = typeof content === "string" ? new TextEncoder().encode(content) : new Uint8Array(content);
        const pointer = Module._malloc(data.length);
        Module.HEAPU8.set(data, pointer);
        try {
            checked(Module.ccall("haylen_web_set_file", "number", ["string", "number", "number"], [path, pointer, data.length]));
        } finally {
            Module._free(pointer);
        }
    };

    haylen.removeFile = function (path) {
        return checked(Module.ccall("haylen_web_remove_file", "number", ["string"], [path])) === 1;
    };

    haylen.run = function () {
        Module._haylen_web_run_files();
    };

    // Starts the last playing app again from its package, which reloads every script.
    haylen.restart = function () {
        Module._haylen_web_restart();
    };

    haylen.stop = function () {
        Module._haylen_web_stop();
    };

    // A paused app keeps its last frame on screen and sees the pause as a suspend. Starting an app always leaves it running.
    haylen.setPaused = function (paused) {
        Module._haylen_web_set_paused(paused ? 1 : 0);
    };

    haylen.pause = function () {
        haylen.setPaused(true);
    };

    haylen.resume = function () {
        haylen.setPaused(false);
    };

    haylen.paused = function () {
        return Module._haylen_web_paused() === 1;
    };

    // Reloads assets read from a file changed with setFile, such as a texture the editor just painted, without restarting the app.
    haylen.reloadAsset = function (path) {
        return checked(Module.ccall("haylen_web_reload_asset", "number", ["string"], [path])) === 1;
    };

    // WebSockets the app opened through haylen.net, by the id the runtime gave them. Their events reach the app on its next frame.
    const sockets = new Map();

    haylen.openSocket = function (id, url, protocols) {
        const report = (name, types, values) => Module.ccall(name, null, ["number", ...types], [id, ...values]);
        let socket;
        try {
            socket = new WebSocket(url, protocols);
        } catch (error) {
            // An address the browser rejects fails later, like any connection that fails.
            setTimeout(() => {
                report("haylen_web_socket_failed", ["string"], [String(error.message)]);
                report("haylen_web_socket_closed", ["number", "string"], [1006, ""]);
            });
            return;
        }
        socket.binaryType = "arraybuffer";
        socket.onopen = () => report("haylen_web_socket_opened", ["string"], [socket.protocol]);
        socket.onerror = () => report("haylen_web_socket_failed", ["string"], ["The WebSocket connection to " + url + " failed."]);
        socket.onclose = (event) => {
            sockets.delete(id);
            report("haylen_web_socket_closed", ["number", "string"], [event.code, event.reason]);
        };
        socket.onmessage = (event) => {
            const binary = typeof event.data !== "string";
            const bytes = binary ? new Uint8Array(event.data) : new TextEncoder().encode(event.data);
            const pointer = Module._malloc(bytes.length);
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

    // Native text editing. A hidden textarea, or a password input, lies over the focused field next to the canvas, so phones open their keyboard and browsers bring their input methods, paste, copy and undo. The engine also lists the fields on screen, because Safari on iOS opens the keyboard only when an element takes the focus inside a user gesture.
    const text = { elements: {}, field: null, fields: [], active: null, composition: null, finishing: false, keyboard: [0, 0, 0, 0] };
    haylen.textInput = text;

    // The engine counts code points, while JavaScript strings count characters outside the Basic Multilingual Plane twice.
    const toCodePoints = (value, index) => {
        let count = 0;
        for (let position = 0; position < index; count++) {
            const code = value.charCodeAt(position);
            position += code >= 0xd800 && code <= 0xdbff && position + 1 < value.length ? 2 : 1;
        }
        return count;
    };

    const toUtf16 = (value, codePoints) => {
        let position = 0;
        for (let count = 0; count < codePoints && position < value.length; count++) {
            const code = value.charCodeAt(position);
            position += code >= 0xd800 && code <= 0xdbff && position + 1 < value.length ? 2 : 1;
        }
        return position;
    };

    // Turns framebuffer pixels of the engine into page pixels over the canvas box.
    const toPage = (bounds) => {
        const box = Module.canvas.getBoundingClientRect();
        const scale = box.width / Module.canvas.width;
        return { left: box.left + bounds[0] * scale, top: box.top + bounds[1] * scale, width: bounds[2] * scale, height: bounds[3] * scale };
    };

    const report = () => {
        const target = text.active;
        const field = text.field;
        if (!field || !target) {
            return;
        }
        const value = target.value;
        const composition = text.composition;
        const start = composition ? toCodePoints(value, composition.start) : -1;
        const end = composition ? toCodePoints(value, Math.min(value.length, composition.start + composition.length)) : -1;
        Module.ccall("haylen_web_text_edited", null, ["number", "number", "string", "number", "number", "number", "number"], [field.id, field.revision, value, toCodePoints(value, target.selectionStart), toCodePoints(value, target.selectionEnd), start, end]);
    };

    // Actions are 0 for submit, 1 for next, 2 for cancel and 3 for a keyboard the user closed.
    const act = (action) => {
        if (text.field) {
            Module.ccall("haylen_web_text_action", null, ["number", "number"], [text.field.id, action]);
        }
    };

    // The keyboard frame is the part of the canvas below the visible viewport, in framebuffer pixels.
    const reportKeyboard = () => {
        const viewport = window.visualViewport;
        let frame = [0, 0, 0, 0];
        if (viewport && text.field && Module.canvas) {
            const box = Module.canvas.getBoundingClientRect();
            const top = Math.max(box.top, viewport.offsetTop + viewport.height);
            if (box.width > 0 && box.bottom - top >= 1) {
                const scale = Module.canvas.width / box.width;
                frame = [0, (top - box.top) * scale, Module.canvas.width, (box.bottom - top) * scale];
            }
        }
        if (frame.some((value, index) => value !== text.keyboard[index])) {
            text.keyboard = frame;
            Module.ccall("haylen_web_keyboard", null, ["number", "number", "number", "number"], frame);
        }
    };

    const isElement = (target) => target === text.elements.textarea || target === text.elements.password;

    // Other elements of the page own the keys they receive, such as the buttons of a plugin dialog or the fields of an editor next to the canvas, while the app owns the keys of the canvas, the body and the page.
    const isForeign = (target) => target instanceof Element && target !== Module.canvas && target !== document.body && target !== document.documentElement && !isElement(target);

    // sokol_app listens for keys on the window in the capture phase, before the element sees them. A field of the UI owns every key, so sokol_app never applies them twice, and its return, tab and escape become actions. The plain keyboard of haylen.window.setKeyboardVisible leaves keys to sokol_app and only takes their text.
    const onKey = (event) => {
        if (isForeign(event.target)) {
            event.stopImmediatePropagation();
            return;
        }
        const field = text.field;
        if (!field || !isElement(event.target)) {
            return;
        }
        if (field.id === 0) {
            if (event.type === "keypress") {
                event.stopImmediatePropagation();
            }
            return;
        }
        event.stopImmediatePropagation();
        if (event.type !== "keydown" || event.isComposing || event.keyCode === 229) {
            return;
        }
        const action = event.key === "Enter" && !field.multiline ? 0 : event.key === "Tab" ? 1 : event.key === "Escape" ? 2 : -1;
        if (action >= 0) {
            event.preventDefault();
            act(action);
        }
    };

    // The focus moving to or from an element, such as a field, the canvas or the button of a plugin dialog, is no focus change of the page, which sokol_app would report as the app losing its focus, so only the focus of the window reaches it. A blur of a field while the page keeps the focus means the user closed the keyboard or left the field, unless the engine ended the editing.
    const onFocus = (event) => {
        if (event.target === window) {
            return;
        }
        event.stopImmediatePropagation();
        if (!isElement(event.target) || event.type !== "blur" || text.finishing) {
            return;
        }
        const target = event.target;
        setTimeout(() => {
            if (text.field && text.field.id !== 0 && text.active === target && document.activeElement !== target && document.hasFocus()) {
                act(3);
            }
        });
    };

    // These listeners are added when the page loads, before sokol_app adds its own, so they run first.
    for (const type of ["keydown", "keyup", "keypress"]) {
        window.addEventListener(type, onKey, true);
    }
    for (const type of ["focus", "blur"]) {
        window.addEventListener(type, onFocus, true);
    }

    const element = (password) => {
        const kind = password ? "password" : "textarea";
        if (text.elements[kind]) {
            return text.elements[kind];
        }
        const target = document.createElement(password ? "input" : "textarea");
        if (password) {
            target.type = "password";
        }
        // A font of 16 pixels keeps Safari on iOS from zooming into the page when the element takes the focus.
        target.style.cssText = "position:fixed;left:0;top:0;width:1px;height:1px;opacity:0;color:transparent;caret-color:transparent;background:transparent;border:0;margin:0;padding:0;outline:none;resize:none;overflow:hidden;font-size:16px;pointer-events:none;z-index:1;";
        target.setAttribute("autocomplete", "off");
        target.addEventListener("input", report);
        target.addEventListener("selectionchange", report);
        target.addEventListener("compositionstart", () => {
            text.composition = { start: target.selectionStart, length: 0 };
        });
        target.addEventListener("compositionupdate", (event) => {
            if (text.composition) {
                text.composition.length = event.data.length;
            }
        });
        target.addEventListener("compositionend", () => {
            text.composition = null;
            report();
        });
        for (const type of ["paste", "copy", "cut"]) {
            target.addEventListener(type, (event) => event.stopPropagation());
        }
        Module.canvas.after(target);
        text.elements[kind] = target;
        return target;
    };

    // Chooses the keyboard and lays the element over the field.
    const configure = (target, field) => {
        target.setAttribute("inputmode", field.inputMode);
        if (field.enterKeyHint) {
            target.setAttribute("enterkeyhint", field.enterKeyHint);
        } else {
            target.removeAttribute("enterkeyhint");
        }
        target.setAttribute("autocapitalize", field.autocapitalize);
        target.setAttribute("autocorrect", field.autocorrect ? "on" : "off");
        target.spellcheck = field.autocorrect;
        if (field.maxLength > 0) {
            target.maxLength = field.maxLength;
        } else {
            target.removeAttribute("maxlength");
        }
        const place = toPage(field.bounds);
        target.style.left = place.left + "px";
        target.style.top = place.top + "px";
        target.style.width = Math.max(place.width, 1) + "px";
        target.style.height = Math.max(place.height, 1) + "px";
    };

    const blurActive = () => {
        if (text.active && document.activeElement === text.active) {
            text.finishing = true;
            text.active.blur();
            text.finishing = false;
        }
    };

    // Starts editing a field, or follows it. The text and the selection change only with a new revision, so an open composition survives.
    text.edit = function (field) {
        const target = element(field.password);
        if (text.active !== target) {
            blurActive();
        }
        configure(target, field);
        const replaced = !text.field || text.field.id !== field.id || text.field.revision !== field.revision || text.active !== target;
        text.active = target;
        text.field = field;
        if (replaced) {
            text.composition = null;
            target.value = field.text;
            target.setSelectionRange(toUtf16(field.text, field.selectionStart), toUtf16(field.text, field.selectionEnd));
        }
        if (document.activeElement !== target) {
            target.focus({ preventScroll: true });
        }
        reportKeyboard();
    };

    text.finish = function () {
        text.field = null;
        text.composition = null;
        blurActive();
        reportKeyboard();
    };

    text.setFields = function (fields) {
        text.fields = fields;
    };

    // A tap on a field focuses its element inside the gesture, before the engine even sees the tap.
    const focusTapped = (event) => {
        for (const field of text.fields) {
            const place = toPage(field.bounds);
            if (event.clientX >= place.left && event.clientX <= place.left + place.width && event.clientY >= place.top && event.clientY <= place.top + place.height) {
                const target = element(field.password);
                configure(target, field);
                if (document.activeElement !== target) {
                    target.focus({ preventScroll: true });
                }
                return;
            }
        }
    };

    // The audio output that BrowserAudioOutput.cpp drives, one per audio device of the engine. The engine mixes blocks of interleaved samples on this thread and posts them to an AudioWorkletNode, whose processor in haylen-audio-worklet.js plays them from a ring buffer. The processor answers with the frames it played and the buffers of the blocks it copied, and the page mixes new blocks until the audio queued ahead of the output is back at its target, so neither shared memory nor threads are needed.
    const audio = { outputs: new Map() };
    haylen.audio = audio;

    // Browsers offer AudioWorklet only to pages served over https or from localhost, and some keep the AudioWorkletNode constructor elsewhere while their contexts have no audioWorklet. Such pages run without sound, and the engine logs why.
    audio.supported = function () {
        return typeof AudioContext !== "undefined" && typeof AudioWorkletNode !== "undefined" && "audioWorklet" in AudioContext.prototype;
    };

    // Browsers run a context only once the user has interacted with the page, unless their autoplay policy lets it run on its own, and Safari on iOS suspends it again after interruptions. A resume without either only makes the browser warn, so the output then waits for the next tap, click or key.
    const resumeAudio = (output) => {
        const activation = navigator.userActivation;
        if (output.context.state === "running" || (activation && !activation.hasBeenActive && !output.ran)) {
            return;
        }
        output.context.resume().catch(() => {});
    };

    const mixBlock = (output) => {
        const samples = output.spare.pop() || new Float32Array(output.blockFrames * output.channels);
        Module._haylen_web_audio_render(output.device, output.scratch, output.blockFrames);
        const first = output.scratch / Float32Array.BYTES_PER_ELEMENT;
        samples.set(Module.HEAPF32.subarray(first, first + samples.length));
        output.node.port.postMessage(samples, [samples.buffer]);
        output.queued += output.blockFrames;
        output.blocks += 1;
    };

    const fillAudio = (output) => {
        while (output.started && output.node && output.queued + output.blockFrames <= output.bufferedFrames) {
            mixBlock(output);
        }
    };

    const connectAudio = (output) => {
        // The ring buffer of the processor holds the whole target, which the page never exceeds.
        const options = { channels: output.channels, capacity: output.bufferedFrames, reportFrames: output.blockFrames };
        output.node = new AudioWorkletNode(output.context, "haylen-audio", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [output.channels], processorOptions: options });
        output.node.port.onmessage = (event) => {
            output.queued -= event.data.played;
            output.underruns = event.data.underruns;
            for (const buffer of event.data.buffers) {
                output.spare.push(new Float32Array(buffer));
            }
            fillAudio(output);
        };
        output.node.connect(output.context.destination);
        if (output.started) {
            output.node.port.postMessage({ active: true });
            fillAudio(output);
        }
    };

    // Creates the context and loads the processor, which may still be loading when the engine starts the output. Returns false when the browser refuses the context.
    audio.open = function (device, channels, sampleRate, blockFrames, bufferedFrames) {
        let context;
        try {
            context = new AudioContext({ sampleRate, latencyHint: "interactive" });
        } catch (error) {
            console.error("The audio output could not be opened: " + error.message);
            return false;
        }
        const output = { device, channels, blockFrames, bufferedFrames, context, node: null, started: false, ran: false, queued: 0, blocks: 0, underruns: 0, spare: [], scratch: Module._malloc(blockFrames * channels * Float32Array.BYTES_PER_ELEMENT) };
        context.onstatechange = () => {
            output.ran = output.ran || context.state === "running";
        };
        audio.outputs.set(device, output);
        // An output that closed while its processor loaded is gone, whatever the loading gave.
        const worklet = locateFile("haylen-audio-worklet.js");
        const isOpen = () => audio.outputs.get(device) === output;
        context.audioWorklet.addModule(worklet).then(
            () => {
                if (isOpen()) {
                    connectAudio(output);
                }
            },
            (error) => {
                if (isOpen()) {
                    console.error("The audio processor " + worklet + " could not be loaded: " + error.message);
                }
            }
        );
        return true;
    };

    // The engine starts and stops the output from inside a frame, so the first blocks are mixed once the frame returned.
    audio.start = function (device) {
        const output = audio.outputs.get(device);
        output.started = true;
        if (output.node) {
            output.node.port.postMessage({ active: true });
        }
        resumeAudio(output);
        queueMicrotask(() => fillAudio(output));
    };

    // The processor keeps what it has queued and plays silence, so the output goes on where it stopped.
    audio.stop = function (device) {
        const output = audio.outputs.get(device);
        output.started = false;
        if (output.node) {
            output.node.port.postMessage({ active: false });
        }
        output.context.suspend();
    };

    audio.close = function (device) {
        const output = audio.outputs.get(device);
        audio.outputs.delete(device);
        if (output.node) {
            output.node.port.onmessage = null;
            output.node.disconnect();
        }
        output.context.close();
        Module._free(output.scratch);
    };

    // Describes the output of the app for onStats, which is unavailable while the app runs without sound.
    audio.stats = function () {
        const [output] = audio.outputs.values();
        if (!output) {
            return { available: false };
        }
        const rate = output.context.sampleRate;
        return { available: true, state: output.context.state, sampleRate: rate, bufferedMilliseconds: (output.queued * 1000) / rate, blocks: output.blocks, underruns: output.underruns };
    };

    // Every tap, click and key resumes the outputs the app keeps playing, unless the app is in the background.
    const unlockAudio = () => {
        if (document.visibilityState !== "visible" || haylen.paused()) {
            return;
        }
        for (const output of audio.outputs.values()) {
            if (output.started) {
                resumeAudio(output);
            }
        }
    };

    const reportNetwork = () => {
        Module._haylen_web_network(navigator.onLine ? 1 : 0);
    };

    // The screen counts as portrait or landscape by the Screen Orientation API, or by the shape of the page where the browser lacks it.
    haylen.orientation = function () {
        const type = screen.orientation ? screen.orientation.type : matchMedia("(orientation: portrait)").matches ? "portrait" : "landscape";
        return type.startsWith("portrait") ? 1 : 0;
    };

    // Takes 0 for landscape, 1 for portrait and 2 for any. Browsers refuse the lock outside fullscreen pages on phones, which leaves the orientation free.
    haylen.lockOrientation = function (value) {
        const orientation = screen.orientation;
        if (!orientation || !orientation.lock) {
            return;
        }
        if (value === 2) {
            orientation.unlock();
            return;
        }
        orientation.lock(value === 1 ? "portrait" : "landscape").catch(() => {});
    };

    // sokol_app follows window resizes only, so a canvas resized by the page layout announces its new size as a window resize. The runtime now takes the work that waited for it.
    Module.postRun = Module.postRun || [];
    Module.postRun.push(function () {
        new ResizeObserver(() => window.dispatchEvent(new Event("resize"))).observe(Module.canvas);
        waiting.runtimeReady = true;
        for (const work of waiting.runtime.splice(0)) {
            work();
        }
    });

    // A hidden page sends the app to the background, a page that goes away makes the user data durable, and the network state reaches the app when it starts and whenever it changes.
    Module.postRun.push(function () {
        Module.canvas.addEventListener("pointerup", focusTapped, true);
        document.addEventListener("selectionchange", () => {
            if (text.active && document.activeElement === text.active) {
                report();
            }
        });
        if (window.visualViewport) {
            window.visualViewport.addEventListener("resize", reportKeyboard);
            window.visualViewport.addEventListener("scroll", reportKeyboard);
        }
        document.addEventListener("visibilitychange", () => {
            Module._haylen_web_visibility(document.visibilityState === "visible" ? 1 : 0);
            unlockAudio();
        });
        window.addEventListener("pagehide", () => Module._haylen_web_page_hidden());
        window.addEventListener("online", reportNetwork);
        window.addEventListener("offline", reportNetwork);
        reportNetwork();
        for (const type of ["pointerdown", "touchend", "keydown"]) {
            window.addEventListener(type, unlockAudio, { capture: true, passive: true });
        }
    });

    // User data lives in IndexedDB under /persistent, loaded before the app starts so saves are there on the first frame.
    Module.preRun = Module.preRun || [];
    Module.preRun.push(function () {
        FS.mkdir("/persistent");
        FS.mount(IDBFS, {}, "/persistent");
        addRunDependency("haylen-user-data");
        FS.syncfs(true, (error) => {
            if (error) {
                console.error("Loading user data failed", error);
            }
            removeRunDependency("haylen-user-data");
        });
    });

    // A page that already holds the zipped package, such as a loader that downloaded it with a progress bar, hands its bytes over as Module.haylen.packageData. The runtime plays it instead of the bundled package, and a page never turns on development mode.
    if (haylen.packageData) {
        Module.arguments = ["/package.zip"];
        Module.preRun.push(function () {
            FS.writeFile("/package.zip", new Uint8Array(haylen.packageData));
            haylen.packageData = null;
        });
    } else if (haylen.packageUrl) {
        // A page can instead name a zipped package with Module.haylen.packageUrl, which is downloaded before the app starts.
        Module.arguments = ["/package.zip"];
        Module.preRun.push(function () {
            addRunDependency("haylen-package");
            fetch(haylen.packageUrl)
                .then((response) => {
                    if (!response.ok) {
                        throw new Error(response.status + " " + response.statusText);
                    }
                    return response.arrayBuffer();
                })
                .then((buffer) => FS.writeFile("/package.zip", new Uint8Array(buffer)))
                .catch((error) => {
                    const message = "The package '" + haylen.packageUrl + "' could not be downloaded, and the browser reported '" + error.message + "'.";
                    console.error(message);
                    haylen.reportError({ message: message, file: "", line: 0, traceback: "" });
                })
                .finally(() => removeRunDependency("haylen-package"));
        });
    }
})(Module.haylen);
