// Page side of the Haylen web runtime, included before the generated module code.
// Pages talk to the running app through `Module.haylen`: native bridge handlers, the contexts of plugin modules with their overlay over the canvas, the editor API that edits the package of the app and swaps it without reloading the page, development with hot reload, and the `onLog`, `onError`, `onStarted`, `onStopped`, `onStats` and `onReloaded` callbacks.

Module.haylen = Module.haylen || {};

(function (haylen) {
    const handlers = new Map();
    const levels = ["debug", "info", "warning", "error"];

    // The system services of the engine: what the browser tells about the device, its color scheme and its battery, a url opened in a new tab and the vibration of phones whose browser offers it. The details of `navigator.userAgentData` and the battery arrive asynchronously, so the page reads them before the runtime starts.
    const device = { details: {}, battery: null, darkScheme: matchMedia("(prefers-color-scheme: dark)") };

    const readDevice = () => {
        const agent = navigator.userAgentData;
        const details = agent && agent.getHighEntropyValues ? agent.getHighEntropyValues(["platformVersion", "model"]) : Promise.resolve({});
        const battery = navigator.getBattery ? navigator.getBattery() : Promise.resolve(null);
        return Promise.allSettled([details, battery]).then(([detailsResult, batteryResult]) => {
            device.details = detailsResult.status === "fulfilled" ? detailsResult.value : {};
            device.battery = batteryResult.status === "fulfilled" ? batteryResult.value : null;
        });
    };

    // The engine reads the device once, as JSON with the keys of `haylen.system.info()` and without the values the browser does not tell.
    haylen.systemInfo = function () {
        const details = device.details;
        const memory = navigator.deviceMemory;
        return JSON.stringify({
            osVersion: details.platform && details.platformVersion ? details.platform + " " + details.platformVersion : undefined,
            deviceModel: details.model || undefined,
            cpuCores: navigator.hardwareConcurrency || undefined,
            memoryBytes: memory ? Math.round(memory * 1073741824) : undefined,
            locale: navigator.language || undefined,
            languages: [...(navigator.languages || [])],
            timeZone: Intl.DateTimeFormat().resolvedOptions().timeZone || undefined,
        });
    };

    const reportTheme = () => {
        Module._haylen_web_theme(device.darkScheme.matches ? 1 : 0);
    };

    // A browser that cannot read the battery reports a full one on mains power, which is also how it reports a device without a battery.
    const reportBattery = () => {
        const battery = device.battery;
        const full = battery.level >= 1 && battery.chargingTime === 0;
        Module._haylen_web_battery(battery.level, battery.charging ? 1 : 0, full ? 1 : 0);
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

    // Registers an async handler for a platform method. It receives the parsed params and a context with the id of the call and a signal that aborts when the app cancels the call or its timeout passes, and it returns any JSON value. A thrown error fails the call with its message and its code and data, or with the code `exception` when it has no code.
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

    // Parameters, results and events cross as JSON with the bytes of every `ArrayBuffer` and `ArrayBuffer` view, such as a `Uint8Array`, in buffers of their own that the JSON refers to as `{"$bytes": N}`, so binary data never turns into text.
    const encodePayload = (value) => {
        const buffers = [];
        const prepare = (item) => {
            if (item instanceof ArrayBuffer || ArrayBuffer.isView(item)) {
                buffers.push(item instanceof ArrayBuffer ? new Uint8Array(item) : new Uint8Array(item.buffer, item.byteOffset, item.byteLength));
                return { $bytes: buffers.length - 1 };
            }
            if (Array.isArray(item)) {
                return item.map(prepare);
            }
            if (item !== null && typeof item === "object" && typeof item.toJSON !== "function") {
                return Object.fromEntries(Object.entries(item).map(([key, element]) => [key, prepare(element)]));
            }
            return item;
        };
        const json = JSON.stringify(prepare(value === undefined ? null : value));
        return { json: json === undefined ? "null" : json, buffers };
    };

    // Every `{"$bytes": N}` of the JSON becomes buffer `N`, a `Uint8Array`.
    const decodePayload = (json, buffers) => {
        return JSON.parse(json, (key, value) => {
            const index = value !== null && typeof value === "object" && !Array.isArray(value) && Object.keys(value).length === 1 ? value.$bytes : undefined;
            return Number.isInteger(index) && index >= 0 && index < buffers.length ? buffers[index] : value;
        });
    };

    // Copies the buffers into wasm memory with a table of an address and a size for each, for the duration of a call that copies them into the engine.
    const withBuffers = (buffers, body) => {
        const addresses = buffers.map((buffer) => {
            const address = Module._malloc(Math.max(1, buffer.length));
            Module.HEAPU8.set(buffer, address);
            return address;
        });
        const table = Module._malloc(Math.max(1, buffers.length) * 8);
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

    const sendEvent = (event, json, buffers, flags) => {
        withBuffers(buffers, (table, count) => Module.ccall("haylen_web_emit", null, ["string", "string", "number", "number", "number"], [event, json, table, count, flags]));
    };

    // Sends an event to the app, which receives it through `haylen.platform.on`, with `ArrayBuffer` and `Uint8Array` values as bytes. An event that nothing listens to is dropped, unless `options.retain` is `true`: then it waits for the first listener of its name. The events of a name with `options.batched` that arrive in one frame reach the app as one list in order. Events sent before the first app started reach it once it starts.
    haylen.emit = function (event, payload, options) {
        const encoded = encodePayload(payload);
        const flags = (options && options.retain ? 1 : 0) | (options && options.batched ? 2 : 0);
        if (!waiting.appStarted) {
            waiting.events.push({ event, json: encoded.json, buffers: encoded.buffers.map((buffer) => buffer.slice()), flags });
            return;
        }
        sendEvent(event, encoded.json, encoded.buffers, flags);
    };

    // The abort controllers of the calls that wait for their handler, so a cancel reaches the handler and its late answer is dropped.
    const pending = new Map();

    // An error with a code keeps its code and data, and any other error fails with the code `exception` and its name in `data.type`, as on the other platforms.
    const describeFailure = function (error) {
        const message = String(error && error.message ? error.message : error);
        if (error && error.code !== undefined) {
            return { message, code: error.code, data: error.data };
        }
        return { message, code: "exception", data: { type: error && error.name ? error.name : typeof error } };
    };

    // The handler receives the bytes of the app as `Uint8Array` values in its parameters, and its result carries `ArrayBuffer` and `Uint8Array` values as bytes. Failures carry JSON alone.
    haylen.dispatch = function (call, method, params, buffers) {
        const reply = (ok, value) => {
            if (!pending.delete(call)) {
                return;
            }
            const encoded = ok ? encodePayload(value) : { json: JSON.stringify(value), buffers: [] };
            withBuffers(encoded.buffers, (table, count) => Module.ccall("haylen_web_resolve", null, ["number", "number", "string", "number", "number"], [call, ok ? 1 : 0, encoded.json, table, count]));
        };
        const handler = handlers.get(method);
        const controller = new AbortController();
        pending.set(call, controller);
        if (!handler) {
            reply(false, { message: "No page handler is registered for \"" + method + "\".", code: "noHandler" });
            return;
        }
        // The handler starts after the frame that made the call, and a cancel in that frame keeps it from starting, as on the other platforms.
        queueMicrotask(async () => {
            if (controller.signal.aborted) {
                return;
            }
            try {
                reply(true, await handler(decodePayload(params, buffers), { call, signal: controller.signal }));
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

    // Screens of plugins: the openers by `<plugin>.<name>`, the screen that shows, and the session storage entry where a redirect keeps its screen for the page that comes back. Every screen has a random token, which the pages of a popup or a redirect carry back to name the screen they answer.
    const screens = { openers: new Map(), current: null };
    const kScreenKey = "haylen.screen";

    const screenFailure = (message, code) => Object.assign(new Error(message), { code });

    // Ends the screen that shows once, which uncovers the app. A result carries `ArrayBuffer` and `Uint8Array` values as bytes, and a failure carries JSON alone.
    const endScreen = (screen, ok, value) => {
        if (screens.current !== screen) {
            return;
        }
        screens.current = null;
        clearInterval(screen.watch);
        if (screen.popup && !screen.popup.closed) {
            screen.popup.close();
        }
        const encoded = ok ? encodePayload(value) : { json: JSON.stringify(value), buffers: [] };
        withBuffers(encoded.buffers, (table, count) => Module.ccall("haylen_web_finish_screen", null, ["number", "number", "string", "number", "number"], [screen.id, ok ? 1 : 0, encoded.json, table, count]));
    };

    // A popup answers with `{haylenScreen: token, result}` or `{haylenScreen: token, error: {message, code, data}}`, which its page posts to the opener, from an origin the screen trusts, or on the `BroadcastChannel` `haylen-screens`, which only pages of this origin reach and which works even when the popup lost its opener.
    const receiveScreenAnswer = (data, trusted) => {
        const screen = screens.current;
        if (!screen || !screen.answer || data === null || typeof data !== "object" || data.haylenScreen !== screen.token || !trusted(screen)) {
            return;
        }
        if ("error" in data) {
            screen.answer.reject(Object.assign(new Error(String(data.error && data.error.message ? data.error.message : data.error)), { code: data.error && data.error.code, data: data.error && data.error.data }));
        } else {
            screen.answer.resolve(data.result === undefined ? null : data.result);
        }
    };

    window.addEventListener("message", (event) => receiveScreenAnswer(event.data, (screen) => screen.origins.includes(event.origin) && event.source === screen.popup));
    if (typeof BroadcastChannel !== "undefined") {
        new BroadcastChannel("haylen-screens").onmessage = (event) => receiveScreenAnswer(event.data, () => true);
    }

    // A page that the browser brings back from its cache after a redirect never got the result of the screen, whose app still waits for it.
    window.addEventListener("pageshow", (event) => {
        const screen = screens.current;
        if (event.persisted && screen && screen.redirected) {
            try {
                sessionStorage.removeItem(kScreenKey);
            } catch (error) {
                console.warn("The session storage could not forget the screen: " + error.message);
            }
            endScreen(screen, false, { message: "The page came back without the result of the screen.", code: "cancelled" });
        }
    });

    // Opens a popup inside the activation of the tap that asked for the screen and returns a promise of its answer, which fails with the code `popupBlocked` when the browser blocks the popup and with the code `cancelled` when the popup closes without an answer.
    const openPopup = (screen, url, options) => {
        const settings = { width: 480, height: 640, origin: location.origin, ...options };
        const left = Math.max(0, (window.screenX || 0) + ((window.outerWidth || settings.width) - settings.width) / 2);
        const top = Math.max(0, (window.screenY || 0) + ((window.outerHeight || settings.height) - settings.height) / 2);
        const popup = window.open(url, "haylen-screen-" + screen.token, "popup,width=" + settings.width + ",height=" + settings.height + ",left=" + Math.round(left) + ",top=" + Math.round(top));
        if (!popup) {
            return Promise.reject(screenFailure("The browser blocked the popup of the screen. Browsers open popups only right after a click, a tap or a key press of the person.", "popupBlocked"));
        }
        screen.popup = popup;
        screen.origins = [location.origin, new URL(settings.origin, location.href).origin];
        return new Promise((resolve, reject) => {
            screen.answer = { resolve, reject };
            screen.watch = setInterval(() => {
                if (popup.closed) {
                    clearInterval(screen.watch);
                    reject(screenFailure("The popup closed before it answered.", "cancelled"));
                }
            }, 250);
        });
    };

    // Keeps the screen where the page that comes back finds it, and leaves the page for the address.
    const redirect = (screen, url) => {
        try {
            sessionStorage.setItem(kScreenKey, JSON.stringify({ id: screen.id, plugin: screen.plugin, screen: screen.name, token: screen.token, state: JSON.parse(screen.state) }));
        } catch (error) {
            endScreen(screen, false, { message: "The session storage could not keep the screen for the redirect: " + error.message, code: "storageUnavailable" });
            return;
        }
        screen.redirected = true;
        location.assign(url);
    };

    // Opens the screen of a plugin in the frame that follows the tap that asked for it, while the tap still counts as an activation, so the opener may open a popup at once. A screen without an opener fails with the code `noHandler`.
    haylen.openScreen = function (id, plugin, name, params, buffers, state) {
        const token = crypto.randomUUID ? crypto.randomUUID() : String(Math.random()).slice(2) + String(Date.now());
        const screen = { id, plugin, name, state, token, controller: new AbortController(), popup: null, origins: [], watch: null, answer: null, redirected: false };
        screens.current = screen;
        const open = screens.openers.get(plugin + "." + name);
        if (!open) {
            endScreen(screen, false, { message: "No page screen is registered for \"" + plugin + "." + name + "\".", code: "noHandler" });
            return;
        }
        const handle = {
            id,
            name,
            token,
            signal: screen.controller.signal,
            resolve: (value) => endScreen(screen, true, value),
            reject: (error) => endScreen(screen, false, describeFailure(error)),
            popup: (url, options) => openPopup(screen, url, options),
            redirect: (url) => redirect(screen, url),
        };
        try {
            const returned = open(decodePayload(params, buffers), handle);
            if (returned && typeof returned.then === "function") {
                returned.then(undefined, handle.reject);
            }
        } catch (error) {
            handle.reject(error);
        }
    };

    // The app gave the screen up: its opener hears it through the signal, a popup of the screen closes, and the screen ends as `cancelled`.
    haylen.cancelScreen = function (id) {
        const screen = screens.current;
        if (screen && screen.id === id) {
            screen.controller.abort();
            endScreen(screen, false, { message: "The app gave the screen up.", code: "cancelled" });
        }
    };

    // The screen that a redirect of the plugin left before this page loaded, which the plugin ends once it read the result from the address of the page. Its end reaches the app as the retained event `<plugin>.screenRestored` with the state that the app gave.
    const takeRestoredScreen = (plugin) => {
        let record = null;
        try {
            record = JSON.parse(sessionStorage.getItem(kScreenKey));
        } catch (error) {
            return null;
        }
        if (!record || record.plugin !== plugin) {
            return null;
        }
        sessionStorage.removeItem(kScreenKey);
        let settled = false;
        const restore = (ok, value) => {
            if (settled) {
                return;
            }
            settled = true;
            const encoded = ok ? encodePayload(value) : { json: JSON.stringify(value), buffers: [] };
            const buffers = encoded.buffers.map((buffer) => buffer.slice());
            whenRuntimeReady(() => withBuffers(buffers, (table, count) => Module.ccall("haylen_web_restore_screen", null, ["string", "string", "string", "number", "string", "number", "number"], [record.plugin, record.screen, JSON.stringify(record.state), ok ? 1 : 0, encoded.json, table, count])));
        };
        return { id: record.id, name: record.screen, token: record.token, resolve: (value) => restore(true, value), reject: (error) => restore(false, describeFailure(error)) };
    };

    // The web parts of plugins by id, each with the listeners of its context that hear app errors. Their ids are the plugins whose native part runs on the web.
    const plugins = new Map();

    haylen.nativePlugins = function () {
        return [...plugins.keys()];
    };

    // Native UI such as a full screen ad covers the app, which the engine then halts and mutes until the last cover ends.
    const coverApp = () => whenRuntimeReady(() => Module._haylen_web_cover_app());
    const uncoverApp = () => whenRuntimeReady(() => Module._haylen_web_uncover_app());

    // The overlay layer lies over the canvas and lets the pointer through everywhere except on the elements of plugins, which it places by the anchors of their placements. An element with `reserve` set reserves the edge it sits on, in canvas pixels, and the engine widens the safe area of the app by it.
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
            throw new Error("The overlay anchor \"" + value.anchor + "\" is unknown. It is one of \"" + Object.keys(anchors).join("\", \"") + "\".");
        }
        return value;
    };

    // The safe area insets of the page in page pixels, as far as they reach into the canvas box.
    const safeInsets = (box) => {
        const [left, top, right, bottom] = haylen.safeAreaInsets().map((value) => value / (window.devicePixelRatio || 1));
        return { left: Math.max(0, left - box.left), top: Math.max(0, top - box.top), right: Math.max(0, right - (window.innerWidth - box.right)), bottom: Math.max(0, bottom - (window.innerHeight - box.bottom)) };
    };

    // Tells the engine only what changed, and insets of `null` release the edge of the item.
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
            throw new Error("The plugin \"" + id + "\" can only place an HTML element over the app.");
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

    // Native dialogs of the engine: messages in the overlay layer, the file picker of the browser, and its save picker or a download. Each answers once through `haylen_web_resolve_dialog`, unless the app gave it up, which closes a message, while a picker of the browser stays open and its answer goes nowhere.
    const dialogs = { open: new Map(), styled: false };
    const kDialogSteps = { Tab: 1, ArrowRight: 1, ArrowLeft: -1 };
    const kActivationMessage = "The browser shows its file pickers only right after a click, a tap or a key press of the user, so the app asks for them from an input handler.";
    const kDialogStyle = [
        ".haylen-dialog-backdrop{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;padding:16px;box-sizing:border-box;background:rgba(0,0,0,.45);pointer-events:auto;font:15px/1.45 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;}",
        ".haylen-dialog{box-sizing:border-box;width:100%;max-width:440px;max-height:100%;overflow:auto;padding:20px 20px 16px;border-radius:12px;border-top:4px solid #0a84ff;background:#fff;color:#1c1c1e;box-shadow:0 16px 48px rgba(0,0,0,.35);}",
        ".haylen-dialog[data-kind=warning]{border-top-color:#ff9f0a;}",
        ".haylen-dialog[data-kind=error]{border-top-color:#ff453a;}",
        ".haylen-dialog h2{margin:0 0 8px;font-size:17px;font-weight:600;}",
        ".haylen-dialog p{margin:0 0 18px;white-space:pre-wrap;overflow-wrap:anywhere;}",
        ".haylen-dialog-buttons{display:flex;flex-wrap:wrap;justify-content:flex-end;gap:8px;}",
        ".haylen-dialog button{font:inherit;min-width:72px;padding:7px 14px;border-radius:8px;border:1px solid #c7c7cc;background:#f2f2f7;color:inherit;cursor:pointer;}",
        ".haylen-dialog button:first-child{border-color:#0a84ff;background:#0a84ff;color:#fff;}",
        ".haylen-dialog button:focus-visible{outline:2px solid #0a84ff;outline-offset:2px;}",
        "@media (prefers-color-scheme:dark){.haylen-dialog{background:#2c2c2e;color:#f2f2f7;}.haylen-dialog button{border-color:#48484a;background:#3a3a3c;}}",
    ].join("");

    const answerDialog = (id, answer) => {
        if (dialogs.open.delete(id)) {
            Module.ccall("haylen_web_resolve_dialog", null, ["number", "string"], [id, JSON.stringify(answer)]);
        }
    };

    const dialogFailure = (message) => ({ failure: { code: "failed", message } });

    const hasActivation = () => !navigator.userActivation || navigator.userActivation.isActive;

    // The message covers the canvas and takes the focus, in the colors of the page. Its first button is the default one, and a press on the backdrop keeps the focus inside.
    const showMessage = (id, dialog, request) => {
        if (!dialogs.styled) {
            const style = document.createElement("style");
            style.textContent = kDialogStyle;
            document.head.appendChild(style);
            dialogs.styled = true;
        }
        const layer = ensureLayer();
        layout();
        const backdrop = document.createElement("div");
        backdrop.className = "haylen-dialog-backdrop";
        backdrop.addEventListener("pointerdown", (event) => {
            if (event.target === backdrop) {
                event.preventDefault();
            }
        });
        const box = document.createElement("div");
        box.className = "haylen-dialog";
        box.dataset.kind = request.messageKind;
        box.setAttribute("role", "alertdialog");
        box.setAttribute("aria-modal", "true");
        if (request.title) {
            const title = document.createElement("h2");
            title.id = "haylen-dialog-title-" + id;
            title.textContent = request.title;
            box.appendChild(title);
            box.setAttribute("aria-labelledby", title.id);
        }
        const text = document.createElement("p");
        text.id = "haylen-dialog-text-" + id;
        text.textContent = request.text;
        box.appendChild(text);
        box.setAttribute("aria-describedby", text.id);

        const previous = document.activeElement;
        dialog.close = () => {
            backdrop.remove();
            if (previous instanceof HTMLElement && previous.isConnected) {
                previous.focus({ preventScroll: true });
            }
        };
        dialog.dismiss = () => {
            dialog.close();
            answerDialog(id, {});
        };
        const row = document.createElement("div");
        row.className = "haylen-dialog-buttons";
        dialog.buttons = request.buttons.map((label, index) => {
            const button = document.createElement("button");
            button.type = "button";
            button.textContent = label;
            button.addEventListener("click", () => {
                dialog.close();
                answerDialog(id, { button: index });
            });
            row.appendChild(button);
            return button;
        });
        box.appendChild(row);
        dialog.box = box;
        backdrop.appendChild(box);
        layer.appendChild(backdrop);
        dialog.buttons[0].focus({ preventScroll: true });
    };

    // The keys of a message stay in it: Escape dismisses it, and Tab and the arrows move between its buttons, whose own keys press them.
    const keyOfDialog = (event) => {
        const box = event.target.closest(".haylen-dialog");
        const dialog = box && [...dialogs.open.values()].find((entry) => entry.box === box);
        if (!dialog) {
            return;
        }
        const buttons = dialog.buttons;
        const step = event.key === "Tab" && event.shiftKey ? -1 : kDialogSteps[event.key] || 0;
        if (event.key === "Escape") {
            event.preventDefault();
            dialog.dismiss();
        } else if (step !== 0) {
            event.preventDefault();
            buttons[(buttons.indexOf(event.target) + step + buttons.length) % buttons.length].focus();
        }
    };

    // The picked files are copied into the folder of the dialog, where the app reads them with `fs`.
    const copyFiles = async (id, files, folder) => {
        try {
            const contents = await Promise.all(files.map((file) => file.arrayBuffer()));
            if (!dialogs.open.has(id)) {
                return;
            }
            FS.mkdirTree(folder);
            const picked = files.map((file, index) => {
                const path = folder + "/" + file.name;
                FS.writeFile(path, new Uint8Array(contents[index]));
                return { name: file.name, path };
            });
            answerDialog(id, { files: picked });
        } catch (error) {
            answerDialog(id, dialogFailure("The picked files could not be read, and the browser reported \"" + error.message + "\"."));
        }
    };

    // Browsers answer a closed picker with the `cancel` event of the input, which older browsers never send.
    const openFiles = (id, dialog, request) => {
        if (!hasActivation()) {
            answerDialog(id, dialogFailure(kActivationMessage));
            return;
        }
        const input = document.createElement("input");
        input.type = "file";
        input.multiple = request.multiple;
        input.accept = request.accept;
        input.style.cssText = "position:fixed;left:0;top:0;width:1px;height:1px;opacity:0;pointer-events:none;";
        dialog.close = () => input.remove();
        input.addEventListener("cancel", () => {
            dialog.close();
            answerDialog(id, {});
        });
        input.addEventListener("change", () => {
            dialog.close();
            copyFiles(id, [...input.files], request.folder);
        });
        document.body.appendChild(input);
        input.click();
    };

    // A download needs no activation, and the browser decides where it goes, so the saved file has no path.
    const downloadFile = (id, name, data) => {
        const url = URL.createObjectURL(new Blob([data], { type: "application/octet-stream" }));
        const link = document.createElement("a");
        link.href = url;
        link.download = name;
        link.style.display = "none";
        document.body.appendChild(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 60000);
        answerDialog(id, { saved: { name } });
    };

    // The save picker of the File System Access API writes where the user picks, and browsers without it download the data. The file keeps no path, since the page never sees one.
    const saveFile = async (id, request, data) => {
        if (!window.showSaveFilePicker) {
            downloadFile(id, request.name, data);
            return;
        }
        if (!hasActivation()) {
            answerDialog(id, dialogFailure(kActivationMessage));
            return;
        }
        let handle;
        try {
            handle = await window.showSaveFilePicker({ suggestedName: request.name, types: request.types });
        } catch (error) {
            answerDialog(id, error.name === "AbortError" ? {} : dialogFailure("The save picker failed, and the browser reported \"" + error.message + "\"."));
            return;
        }
        if (!dialogs.open.has(id)) {
            return;
        }
        try {
            const writable = await handle.createWritable();
            await writable.write(data);
            await writable.close();
            answerDialog(id, { saved: { name: handle.name } });
        } catch (error) {
            answerDialog(id, dialogFailure("The file \"" + handle.name + "\" could not be written, and the browser reported \"" + error.message + "\"."));
        }
    };

    // The engine shows a dialog from inside a frame, while the activation of the tap that asked for it still counts. The data of a save arrives as bytes.
    haylen.showDialog = function (id, request, data) {
        const dialog = { close: () => {} };
        dialogs.open.set(id, dialog);
        if (request.kind === "message") {
            showMessage(id, dialog, request);
        } else if (request.kind === "openFiles") {
            openFiles(id, dialog, request);
        } else {
            saveFile(id, request, data);
        }
    };

    haylen.cancelDialog = function (id) {
        const dialog = dialogs.open.get(id);
        if (dialog) {
            dialogs.open.delete(id);
            dialog.close();
        }
    };

    // The size of what a video stream copies, from the natural size of each kind of source.
    const sourceSize = (source) => {
        if (typeof VideoFrame !== "undefined" && source instanceof VideoFrame) {
            return [source.displayWidth, source.displayHeight];
        }
        if (source instanceof HTMLVideoElement) {
            return [source.videoWidth, source.videoHeight];
        }
        if (source instanceof HTMLImageElement) {
            return [source.naturalWidth, source.naturalHeight];
        }
        return [source.width, source.height];
    };

    // The timestamp in seconds of a `VideoFrame`, of the current frame of a video, or of the moment of the push.
    const sourceTime = (source) => {
        if (typeof VideoFrame !== "undefined" && source instanceof VideoFrame && source.timestamp !== null) {
            return source.timestamp / 1e6;
        }
        return source instanceof HTMLVideoElement ? source.currentTime : performance.now() / 1000;
    };

    // Opens a stream of the engine once the runtime is ready, and logs why when the engine refuses it.
    const openStream = (stream, open) => {
        whenRuntimeReady(() => {
            stream.handle = open();
            if (!stream.handle) {
                console.error(Module.UTF8ToString(Module._haylen_web_last_error()));
            }
        });
    };

    // Keeps a block of wasm memory of at least `size` bytes for the pushes of a stream.
    const reserveMemory = (stream, size) => {
        if (stream.size < size) {
            Module._free(stream.memory);
            stream.memory = Module._malloc(size);
            stream.size = size;
        }
        return stream.memory;
    };

    // A video stream draws each source into a canvas of its size and copies the RGBA pixels into wasm memory, where the engine keeps the newest frame for its texture. Pushes before the runtime is ready, and of a video without its first frame yet, are dropped and return `false`.
    const openVideoStream = (id, name) => {
        const stream = { handle: 0, canvas: null, context: null, memory: 0, size: 0 };
        openStream(stream, () => Module.ccall("haylen_web_open_video_stream", "number", ["string", "string"], [id, name]));
        return {
            push(source, timestamp) {
                const [width, height] = sourceSize(source);
                if (!stream.handle || !(width > 0 && height > 0)) {
                    return false;
                }
                if (!stream.canvas || stream.canvas.width !== width || stream.canvas.height !== height) {
                    stream.canvas = new OffscreenCanvas(width, height);
                    stream.context = stream.canvas.getContext("2d", { willReadFrequently: true });
                }
                stream.context.clearRect(0, 0, width, height);
                stream.context.drawImage(source, 0, 0, width, height);
                const pixels = stream.context.getImageData(0, 0, width, height).data;
                Module.HEAPU8.set(pixels, reserveMemory(stream, pixels.length));
                return checked(Module._haylen_web_push_video_frame(stream.handle, stream.memory, width, height, timestamp === undefined ? sourceTime(source) : timestamp)) === 1;
            },
        };
    };

    // An audio stream copies interleaved `Float32Array` samples into its ring in wasm memory, and `push` returns how many frames fit. The capacity defaults to one second of frames.
    const openAudioStream = (id, name, options) => {
        const { sampleRate, channels, capacity } = { capacity: options.sampleRate, ...options };
        const stream = { handle: 0, memory: 0, size: 0 };
        openStream(stream, () => Module.ccall("haylen_web_open_audio_stream", "number", ["string", "string", "number", "number", "number"], [id, name, sampleRate, channels, capacity]));
        return {
            push(samples) {
                if (samples.length % channels !== 0) {
                    throw new Error("Audio samples come in whole frames, one sample for every channel.");
                }
                if (!stream.handle) {
                    return 0;
                }
                const memory = reserveMemory(stream, samples.length * Float32Array.BYTES_PER_ELEMENT);
                Module.HEAPF32.set(samples, memory / Float32Array.BYTES_PER_ELEMENT);
                return checked(Module._haylen_web_push_audio_frames(stream.handle, memory, samples.length / channels));
            },
        };
    };

    // What a plugin needs from the page: a secure context, an API of the browser, which is a path of properties from the global object such as `navigator.contacts`, and a feature that the permissions policy of the page allows. Each missing requirement logs once per plugin, and none of them has a file of a project that adds it, so `file` stays empty, while `snippet` holds the text that allows a feature of the permissions policy.
    const requirements = { kinds: ["secureContext", "api", "permissionsPolicy"], reported: new Set() };

    const hasApi = (path) => path.split(".").reduce((value, part) => (value === undefined || value === null ? undefined : value[part]), globalThis) != null;

    // Browsers that do not tell the policy of the page, such as Firefox and Safari, leave the error of the API to tell instead.
    const allowsFeature = (feature) => {
        const policy = document.permissionsPolicy || document.featurePolicy;
        return !policy || policy.allowsFeature(feature);
    };

    // A page inside a frame gets a feature from the `allow` attribute of its frame, and a page on its own from the `Permissions-Policy` header it comes with.
    const describeRequirement = (kind, name) => {
        if (kind === "secureContext") {
            return { entry: { kind, name, file: "", snippet: "" }, summary: "a secure context", instructions: "Serve the page over \"https\" or from \"localhost\"." };
        }
        if (kind === "api") {
            return { entry: { kind, name, file: "", snippet: "" }, summary: "the API \"" + name + "\"", instructions: "Run the app in a browser that offers it." };
        }
        const framed = window.self !== window.top;
        const snippet = framed ? "allow=\"" + name + "\"" : "Permissions-Policy: " + name + "=(self)";
        const instructions = framed ? "Add \"" + snippet + "\" to the \"<iframe>\" that embeds the page." : "Send \"" + snippet + "\" with the page.";
        return { entry: { kind, name, file: "", snippet }, summary: "the permissions policy feature \"" + name + "\"", instructions };
    };

    // Throws a failure with the code `unsupported`, whose data lists each missing requirement in `missing` as `{kind, name, file, snippet}`, after it logged each missing one once.
    const requirePage = (id, needs) => {
        for (const key of Object.keys(needs)) {
            if (!requirements.kinds.includes(key)) {
                throw new TypeError("A plugin requires \"secureContext\", \"api\" or \"permissionsPolicy\", not \"" + key + "\".");
            }
        }
        const missing = [];
        if (needs.secureContext && !window.isSecureContext) {
            missing.push(describeRequirement("secureContext", "https"));
        }
        for (const api of [].concat(needs.api || []).filter((path) => !hasApi(path))) {
            missing.push(describeRequirement("api", api));
        }
        for (const feature of [].concat(needs.permissionsPolicy || []).filter((name) => !allowsFeature(name))) {
            missing.push(describeRequirement("permissionsPolicy", feature));
        }
        if (missing.length === 0) {
            return;
        }

        const owner = "The plugin \"" + id + "\"";
        for (const requirement of missing) {
            const key = id + "\n" + requirement.entry.kind + "\n" + requirement.entry.name;
            if (!requirements.reported.has(key)) {
                requirements.reported.add(key);
                const message = owner + " needs " + requirement.summary + ", which the page lacks, so the calls that need it fail with the code \"unsupported\". " + requirement.instructions;
                console.warn(message);
                notify("onLog", "warning", message);
            }
        }
        const summaries = missing.map((requirement) => requirement.summary);
        const list = summaries.length === 1 ? summaries[0] : summaries.slice(0, -1).join(", ") + " and " + summaries[summaries.length - 1];
        throw Object.assign(new Error(owner + " needs " + list + ", which the page lacks."), { code: "unsupported", data: { missing: missing.map((requirement) => requirement.entry) } });
    };

    // Makes the context that the web module of a plugin receives in `load(context)`. Methods and events take the id of the plugin in front of their names, as the Lua handle of the plugin expects. The loader calls it for every plugin before the runtime starts.
    haylen.createPluginContext = function (id, config) {
        if (plugins.has(id)) {
            throw new Error("The plugin \"" + id + "\" already has a context.");
        }
        const plugin = { errorListeners: [] };
        plugins.set(id, plugin);
        return {
            id,
            config: config || {},
            register(method, handler) {
                haylen.register(id + "." + method, handler);
            },
            registerScreen(name, open) {
                screens.openers.set(id + "." + name, open);
            },
            restoredScreen: takeRestoredScreen(id),
            emit(event, payload, options) {
                haylen.emit(id + "." + event, payload, options);
            },
            videoStream(name) {
                return openVideoStream(id, name);
            },
            audioStream(name, options) {
                return openAudioStream(id, name, options);
            },
            overlay: {
                add: (element, placement) => addToOverlay(id, element, placement),
            },
            coverApp,
            uncoverApp,
            onAppError(listener) {
                plugin.errorListeners.push(listener);
            },
            require(needs) {
                requirePage(id, needs);
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

    // Varn also prints every line to `stdout`, which already reaches the browser console, so pages only get the structured copy here.
    haylen.reportLog = function (level, message) {
        notify("onLog", levels[level] || "info", message);
    };

    // Errors arrive as `{message, file, line, traceback, frames}`, with an empty `file` when no script position is known. The page and every plugin that listens with `onAppError` receive them.
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
                for (const entry of waiting.events.splice(0)) {
                    sendEvent(entry.event, entry.json, entry.buffers, entry.flags);
                }
            });
        }
        notify("onStarted", app);
    };

    haylen.reportStopped = function () {
        notify("onStopped");
    };

    // Reports of hot reload arrive after every batch of changes that applied, as `{restarted: false, mode, modules, assets, resumed, milliseconds}`, or as `{restarted: true, reason}` before a restart.
    haylen.reportReloaded = function (report) {
        notify("onReloaded", report);
    };

    // Statistics arrive about once per second while the app runs: fps, frame times, renderer counters, cached assets, audio voices, profiler scopes and the audio output.
    haylen.reportStats = function (stats) {
        stats.audio = audio.stats();
        notify("onStats", stats);
    };

    haylen.canvasSelector = function () {
        if (!(Module.canvas instanceof HTMLCanvasElement) || !Module.canvas.id) {
            throw new Error("The property \"Module.canvas\" must be a canvas element with an id.");
        }
        return "#" + CSS.escape(Module.canvas.id);
    };

    // Reads `env(safe-area-inset-*)` of the viewport through a hidden probe and returns the part of each inset that covers the canvas, in framebuffer pixels, so a canvas away from the edges that a cutout or the bars of the system cover has none.
    haylen.safeAreaInsets = function () {
        let probe = document.getElementById("haylen-safe-area");
        if (!probe) {
            probe = document.createElement("div");
            probe.id = "haylen-safe-area";
            probe.style.cssText = "position:fixed;visibility:hidden;pointer-events:none;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);";
            document.body.appendChild(probe);
        }
        const style = getComputedStyle(probe);
        const box = Module.canvas.getBoundingClientRect();
        const scale = box.width > 0 ? Module.canvas.width / box.width : 0;
        const covered = [
            parseFloat(style.paddingLeft) - box.left,
            parseFloat(style.paddingTop) - box.top,
            parseFloat(style.paddingRight) - (window.innerWidth - box.right),
            parseFloat(style.paddingBottom) - (window.innerHeight - box.bottom),
        ];
        return covered.map((inset) => Math.max(0, inset || 0) * scale);
    };

    haylen.persist = function () {
        FS.syncfs(false, (error) => {
            if (error) {
                console.error("Saving user data failed.", error);
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

    // Editor API: `setFile` and `removeFile` edit the package that plays, `applyChanges` applies the files edited since its last call the way hot reload does in development and restarts the app otherwise, `run` starts the package again from `source/main.lua`, and `clearFiles` stops the app and empties the package for an app built file by file.
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

    haylen.applyChanges = function () {
        Module._haylen_web_apply_changes();
    };

    // WebSockets the app opened through `haylen.net`, by the id the runtime gave them. Their events reach the app on its next frame.
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
        socket.onerror = () => report("haylen_web_socket_failed", ["string"], ["The WebSocket connection to \"" + url + "\" failed."]);
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

    // The keys whose press reached `sokol_app`, whose release reaches it too wherever the focus went in between, such as to the field that the press started editing, so no key stays held.
    const held = new Set();

    // The library `sokol_app` listens for keys on the window in the capture phase, before the element sees them. A field of the UI owns every key, so `sokol_app` never applies them twice, and its return, tab and escape become actions. The plain keyboard of `haylen.window.setKeyboardVisible` leaves keys to `sokol_app` and only takes their text.
    const onKey = (event) => {
        if (event.type === "keyup" && held.delete(event.code)) {
            return;
        }
        if (isForeign(event.target)) {
            event.stopImmediatePropagation();
            if (event.type === "keydown") {
                keyOfDialog(event);
            }
            return;
        }
        const field = text.field;
        if (!field || !isElement(event.target) || (field.id === 0 && event.type !== "keypress")) {
            if (event.type === "keydown") {
                held.add(event.code);
            }
            return;
        }
        event.stopImmediatePropagation();
        if (field.id === 0 || event.type !== "keydown" || event.isComposing || event.keyCode === 229) {
            return;
        }
        const action = event.key === "Enter" && !field.multiline ? 0 : event.key === "Tab" ? 1 : event.key === "Escape" ? 2 : -1;
        if (action >= 0) {
            event.preventDefault();
            act(action);
        }
    };

    // The focus moving to or from an element, such as a field, the canvas or the button of a plugin dialog, is no focus change of the page, which `sokol_app` would report as the app losing its focus, so only the focus of the window reaches it. The engine releases every held key when the window loses the focus, so the page forgets them then too. A blur of a field while the page keeps the focus means the user closed the keyboard or left the field, unless the engine ended the editing.
    const onFocus = (event) => {
        if (event.target === window) {
            if (event.type === "blur") {
                held.clear();
            }
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

    // These listeners are added when the page loads, before `sokol_app` adds its own, so they run first.
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

    // The audio output that `BrowserAudioOutput.cpp` drives, one per audio device of the engine. The engine mixes blocks of interleaved samples on this thread and posts them to an `AudioWorkletNode`, whose processor in `haylen-audio-worklet.js` plays them from a ring buffer. The processor answers with the frames it played and the buffers of the blocks it copied, and the page mixes new blocks until the audio queued ahead of the output is back at its target, so neither shared memory nor threads are needed.
    const audio = { outputs: new Map() };
    haylen.audio = audio;

    // Browsers offer AudioWorklet only to pages served over https or from localhost, and some keep the `AudioWorkletNode` constructor elsewhere while their contexts have no `audioWorklet`. Such pages run without sound, and the engine logs why.
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

    // Creates the context and loads the processor, which may still be loading when the engine starts the output. Returns `false` when the browser refuses the context.
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
                    console.error("The audio processor \"" + worklet + "\" could not be loaded: " + error.message);
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

    // Describes the output of the app for `onStats`, which is unavailable while the app runs without sound.
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

    // The library `sokol_app` follows window resizes only, so a canvas resized by the page layout announces its new size as a window resize. The runtime now takes the work that waited for it.
    Module.postRun = Module.postRun || [];
    Module.postRun.push(function () {
        new ResizeObserver(() => window.dispatchEvent(new Event("resize"))).observe(Module.canvas);
        waiting.runtimeReady = true;
        for (const work of waiting.runtime.splice(0)) {
            work();
        }
    });

    // A hidden page sends the app to the background, a page that goes away makes the user data durable, and the network state, the color scheme and the battery reach the app when it starts and whenever they change.
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
        reportTheme();
        device.darkScheme.addEventListener("change", reportTheme);
        if (device.battery) {
            reportBattery();
            for (const type of ["levelchange", "chargingchange", "chargingtimechange"]) {
                device.battery.addEventListener(type, reportBattery);
            }
        }
        for (const type of ["pointerdown", "touchend", "keydown"]) {
            window.addEventListener(type, unlockAudio, { capture: true, passive: true });
        }
    });

    // User data lives in IndexedDB under `/persistent`, loaded before the app starts so saves are there on the first frame, and so is what the browser tells about the device.
    Module.preRun = Module.preRun || [];
    Module.preRun.push(function () {
        addRunDependency("haylen-device");
        readDevice().then(() => removeRunDependency("haylen-device"));
    });
    Module.preRun.push(function () {
        FS.mkdir("/persistent");
        FS.mount(IDBFS, {}, "/persistent");
        addRunDependency("haylen-user-data");
        FS.syncfs(true, (error) => {
            if (error) {
                console.error("Loading user data failed.", error);
            }
            removeRunDependency("haylen-user-data");
        });
    });

    // A page that already holds the zipped package, such as a loader that downloaded it with a progress bar, hands its bytes over as `Module.haylen.packageData`, and a page can instead name a zipped package with `Module.haylen.packageUrl`, which is downloaded before the app starts. The runtime plays it instead of the bundled package. Only a page that sets `Module.haylen.development` runs the app in development, connected to the development server that `Module.haylen.developmentServer` names, if any.
    const launch = [];
    if (haylen.packageData || haylen.packageUrl) {
        launch.push("/package.zip");
    }
    if (haylen.development) {
        launch.push("--dev");
    }
    if (haylen.developmentServer) {
        launch.push("--dev-server", haylen.developmentServer);
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
                    return response.arrayBuffer();
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
})(Module.haylen);
