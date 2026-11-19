// Page side of the Haylen web runtime: what needs the document of the page, which are the input and the size of the canvas, the hidden text fields, the overlay of plugins, the mirror of the UI for screen readers, the clipboard, fullscreen, the pointer lock, the wake lock, the now-playing controls, the elements of videos, the windows of outputs, the audio contexts and the web modules of plugins with their permissions. When the module runs on the page, the build includes this script after the engine side of `haylen-runtime.js`. In the Worker mode the page loads this script alone, and it starts the module in a dedicated worker, which it hands the canvas as an `OffscreenCanvas` and reaches through messages.
// Pages talk to the running app through `Module.haylen`: native bridge handlers, the contexts of plugin modules with their overlay over the canvas, the versioned editor contract of `Module.haylen.editor`, which edits the package of the app and runs it again without reloading the page, development with hot reload, and the `onLog`, `onError`, `onLoadProgress`, `onStarted`, `onSplashEnded`, `onStopped`, `onStats` and `onReloaded` callbacks.

Module.haylen = Module.haylen || {};

(function (haylen) {
    // The worker of the Worker mode runs the engine side alone.
    if (typeof document === "undefined") {
        return;
    }

    if (haylen.hosting !== undefined && haylen.hosting !== "worker") {
        throw new Error("The field \"Module.haylen.hosting\" is \"worker\" for the Worker mode, or absent for the runtime on the page.");
    }

    const hostedInWorker = haylen.hosting === "worker";

    // The engine side: `haylen.engine` of `haylen-runtime.js` on the page, and the link to the worker in the Worker mode, which the end of this script makes.
    let engine = haylen.engine;

    // The files of the runtime lie next to its script, whose folder Emscripten knows on the page, while in the Worker mode the page names the script.
    const runtimeFile = (name) => (hostedInWorker ? new URL(name, new URL(haylen.runtimeUrl || "haylen.js", document.baseURI)).href : locateFile(name));

    const handlers = new Map();
    // The size of the drawable that the engine draws into, in pixels, which `haylen.setDrawableWidth` keeps up to date.
    const drawable = { width: 0 };
    const levels = ["debug", "info", "warning", "error"];

    // The system services of the engine: what the browser tells about the device, its color scheme, its accessibility preferences, its battery and its time zone. The details of `navigator.userAgentData` and the battery arrive asynchronously, so the page reads them before the runtime starts.
    const device = {
        details: {},
        battery: null,
        darkScheme: matchMedia("(prefers-color-scheme: dark)"),
        reducedMotion: matchMedia("(prefers-reduced-motion: reduce)"),
        highContrast: matchMedia("(prefers-contrast: more)"),
        reducedTransparency: matchMedia("(prefers-reduced-transparency: reduce)"),
    };

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
        });
    };

    const reportTheme = () => {
        engine.call("haylen_web_theme", null, ["number"], [device.darkScheme.matches ? 1 : 0]);
    };

    // Browsers announce no change of the time zone, so the page reads it again whenever the person comes back to it, which is when the system may have changed it.
    const reportTimeZone = () => {
        engine.call("haylen_web_time_zone", null, ["string"], [Intl.DateTimeFormat().resolvedOptions().timeZone || ""]);
    };

    // The browser lists the languages of its settings, most preferred first, and older browsers name only one. The engine reads them when it starts, and the page reports every later change.
    haylen.languages = function () {
        const languages = navigator.languages && navigator.languages.length > 0 ? [...navigator.languages] : [navigator.language];
        return JSON.stringify(languages);
    };

    const reportLanguages = () => {
        engine.call("haylen_web_languages", null, ["string"], [haylen.languages()]);
    };

    // The text scale is the default font size of the browser, which the user sets in its preferences, over the usual 16 pixels. Browsers without the media queries of reduced transparency report no preference.
    const reportAccessibility = () => {
        const probe = document.createElement("div");
        probe.style.cssText = "position:absolute;visibility:hidden;font-size:medium";
        document.body.appendChild(probe);
        const fontSize = parseFloat(getComputedStyle(probe).fontSize) || 16;
        probe.remove();
        engine.call("haylen_web_accessibility", null, ["number", "number", "number", "number"], [fontSize / 16, device.reducedMotion.matches ? 1 : 0, device.highContrast.matches ? 1 : 0, device.reducedTransparency.matches ? 1 : 0]);
    };

    // A browser that cannot read the battery reports a full one on mains power, which is also how it reports a device without a battery.
    const reportBattery = () => {
        const battery = device.battery;
        const full = battery.level >= 1 && battery.chargingTime === 0;
        engine.call("haylen_web_battery", null, ["number", "number", "number"], [battery.level, battery.charging ? 1 : 0, full ? 1 : 0]);
    };

    // The now-playing controls of the browser through the Media Session API, whose actions the media keys of keyboards and headsets send too. The commands reach the engine in the order of `MediaSession::Command`, and the play and pause actions send the toggle of an app that offers only the toggle, since browsers have no toggle action.
    const mediaActions = ["play", "pause", null, "stop", "nexttrack", "previoustrack", "seekto"];
    let mediaArtwork = null;
    haylen.nowPlaying = function (info, artwork) {
        if (!("mediaSession" in navigator)) {
            return;
        }

        const session = navigator.mediaSession;

        if (mediaArtwork) {
            URL.revokeObjectURL(mediaArtwork);
            mediaArtwork = null;
        }

        const offers = (command) => info !== null && (info.commands & (1 << command)) !== 0;
        mediaActions.forEach(function (action, command) {
            if (action === null) {
                return;
            }

            const sent = command <= 1 && !offers(command) && offers(2) ? 2 : command;
            const handler = offers(sent) ? (details) => engine.call("haylen_web_media_command", null, ["number", "number"], [sent, details.seekTime || 0]) : null;
            // A browser that lacks an action throws, and the action is then not offered there.
            try {
                session.setActionHandler(action, handler);
            } catch (error) {
                return;
            }
        });

        if (info === null) {
            session.metadata = null;
            session.playbackState = "none";
            return;
        }

        if (artwork.length > 0) {
            mediaArtwork = URL.createObjectURL(new Blob([artwork]));
        }

        session.metadata = new MediaMetadata({ title: info.title, artist: info.artist, album: info.album, artwork: mediaArtwork ? [{ src: mediaArtwork }] : [] });
        session.playbackState = info.playing ? "playing" : "paused";
        if (info.duration > 0 && session.setPositionState) {
            session.setPositionState({ duration: info.duration, position: Math.min(info.position, info.duration), playbackRate: 1 });
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

    // The events of each name that wait for the first app, the newest of them, so a plugin that goes on sending while no app starts never fills the memory.
    const kKeptEventsPerName = 32;

    const whenRuntimeReady = (work) => {
        if (waiting.runtimeReady) {
            work();
            return;
        }

        waiting.runtime.push(work);
    };

    // Parameters, results and events cross as JSON with the bytes of every `ArrayBuffer` and `ArrayBuffer` view, such as a `Uint8Array`, in buffers of their own that the JSON refers to as `{"$bytes": N}`, so binary data never turns into text. A `VarnDataView` crosses as the token of its view, which shares the memory of the heap without a copy, and the payload lists the views so a sender that keeps the payload keeps them. In the Worker mode the heap lives in the worker, so the page has no data views, and a view of the engine that reaches the page fails the call or the screen that carries it with the code `unsupported`.
    const VarnView = hostedInWorker ? null : Module.VarnDataView;
    const isViewToken = (value) => value !== null && typeof value === "object" && Object.hasOwn(value, "$view");
    const encodePayload = (value, views = []) => {
        const buffers = [];
        const prepare = (item) => {
            if (VarnView !== null && item instanceof VarnView) {
                views.push(item);
                return item.token();
            }

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
        return { json: json === undefined ? "null" : json, buffers, views };
    };

    // A result or a failure as the engine reads it. A result that JSON cannot carry, such as one with a `BigInt` or a cycle, fails with the reason, as a handler that threw does, and lets go of the views that it handed over.
    const encodeAnswer = (ok, value) => {
        const views = [];
        try {
            if (!ok) {
                return { ok, json: JSON.stringify(value), buffers: [], views };
            }

            const encoded = encodePayload(value, views);
            return { ok, ...encoded };
        } catch (error) {
            releaseViews(views);
            return { ok: false, json: JSON.stringify(describeFailure(error)), buffers: [], views: [] };
        }
    };

    // Every `{"$bytes": N}` of the JSON becomes buffer `N`, a `Uint8Array`, and every token a `VarnDataView` with a reference of its own, which `acquired` lists so its owner releases it. The engine keeps the views of the JSON alive while it hands the JSON over, so the decode runs before that returns.
    const decodePayload = (json, buffers, acquired) => {
        return JSON.parse(json, (key, value) => {
            if (isViewToken(value) && VarnView === null) {
                throw Object.assign(new Error("The memory of the engine stays in its worker in the Worker mode, so a data view of the app cannot reach the web part of a plugin on the page."), { code: "unsupported" });
            }

            if (isViewToken(value)) {
                const view = VarnView.acquire(value);
                if (view === null) {
                    throw new Error(`The data view ${value.$view} was gone before the page received it.`);
                }

                acquired.push(view);
                return view;
            }

            const index = value !== null && typeof value === "object" && !Array.isArray(value) && Object.keys(value).length === 1 ? value.$bytes : undefined;
            return Number.isInteger(index) && index >= 0 && index < buffers.length ? buffers[index] : value;
        });
    };

    const releaseViews = (views) => views.forEach((view) => view.release());

    const sendEvent = (event, json, buffers, flags) => {
        engine.call("haylen_web_emit", null, ["string", "string", "buffers", "number"], [event, json, buffers, flags]);
    };

    // Sends an event to the app, which receives it through `haylen.platform.on`, with `ArrayBuffer` and `Uint8Array` values as bytes. An event that nothing listens to is dropped, unless `options.retain` is `true`: then it waits for the first listener of its name. The events of a name with `options.batched` that arrive in one frame reach the app as one list in order. Events sent while no app runs, before the first app started or while an app restarts, reach the next app once it starts, up to 32 of each name with the oldest dropped first.
    haylen.emit = function (event, payload, options) {
        const encoded = encodePayload(payload);
        const flags = (options && options.retain ? 1 : 0) | (options && options.batched ? 2 : 0);
        if (!waiting.appStarted) {
            const kept = waiting.events.filter((entry) => entry.event === event);
            if (kept.length === kKeptEventsPerName) {
                releaseViews(kept[0].views);
                waiting.events.splice(waiting.events.indexOf(kept[0]), 1);
            }

            waiting.events.push({ event, json: encoded.json, buffers: encoded.buffers.map((buffer) => buffer.slice()), views: encoded.views.map((view) => view.retain()), flags });
            return;
        }

        sendEvent(event, encoded.json, encoded.buffers, flags);
    };

    // Hands the app a link that opened the page, such as a link of a URL scheme of a plugin that the address of the page stands in for. The app hears it as `linkOpened`, as a link reaches the app on the other platforms, so a link that arrives while a launched app plays ends that app and reaches the app that launched it.
    haylen.openLink = function (url) {
        whenRuntimeReady(() => engine.call("haylen_web_open_link", null, ["string"], [String(url)]));
    };

    // The abort controllers of the calls that wait for their handler, so a cancel reaches the handler and its late answer is dropped.
    const pending = new Map();

    // An error whose code is a text keeps its code and data, and any other error fails with the code `exception` and its name in `data.type`, as on the other platforms. A `DOMException` carries a number as its code, a legacy of the DOM that names no failure.
    const describeFailure = function (error) {
        const message = String(error && error.message ? error.message : error);
        if (error && typeof error.code === "string") {
            return { message, code: error.code, data: error.data };
        }

        return { message, code: "exception", data: { type: error && error.name ? error.name : typeof error } };
    };

    // The handler receives the bytes of the app as `Uint8Array` values in its parameters and its views as `VarnDataView` values, and its result carries `ArrayBuffer` and `Uint8Array` values as bytes and `VarnDataView` values as views. Failures carry JSON alone. The views of the parameters belong to the call until its handler settles, even after a cancel, so a handler keeps one longer with `retain`, and a view in the result is handed over to the engine, so a handler that still needs it returns a `retain` of it.
    haylen.dispatch = function (call, method, params, buffers) {
        const views = [];
        const reply = (ok, value) => {
            if (!pending.delete(call)) {
                return;
            }

            const encoded = encodeAnswer(ok, value);
            engine.call("haylen_web_resolve", null, ["number", "number", "string", "buffers"], [call, encoded.ok ? 1 : 0, encoded.json, encoded.buffers]);
            releaseViews(encoded.views);
        };
        const handler = handlers.get(method);
        const controller = new AbortController();
        pending.set(call, controller);

        if (!handler) {
            reply(false, { message: "No page handler is registered for \"" + method + "\".", code: "noHandler" });
            return;
        }

        let decoded;
        try {
            decoded = decodePayload(params, buffers, views);
        } catch (error) {
            releaseViews(views);
            reply(false, describeFailure(error));
            return;
        }

        // The handler starts after the frame that made the call, and a cancel in that frame keeps it from starting, as on the other platforms.
        queueMicrotask(async () => {
            if (controller.signal.aborted) {
                releaseViews(views);
                return;
            }

            try {
                reply(true, await handler(decoded, { call, signal: controller.signal }));
            } catch (error) {
                reply(false, describeFailure(error));
            } finally {
                releaseViews(views);
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

    // Ends the screen that shows once, which uncovers the app. A result carries `ArrayBuffer` and `Uint8Array` values as bytes and hands its `VarnDataView` values over to the engine, and a failure carries JSON alone.
    const endScreen = (screen, ok, value) => {
        if (screens.current !== screen) {
            return;
        }

        screens.current = null;
        clearInterval(screen.watch);

        if (screen.popup && !screen.popup.closed) {
            screen.popup.close();
        }

        const encoded = encodeAnswer(ok, value);
        engine.call("haylen_web_finish_screen", null, ["number", "number", "string", "buffers"], [screen.id, encoded.ok ? 1 : 0, encoded.json, encoded.buffers]);
        releaseViews(encoded.views);
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
        // The views of the parameters live until the opener returns, so an opener keeps one longer with `retain`.
        const views = [];
        try {
            const returned = open(decodePayload(params, buffers, views), handle);
            if (returned && typeof returned.then === "function") {
                returned.then(undefined, handle.reject);
            }
        } catch (error) {
            handle.reject(error);
        } finally {
            releaseViews(views);
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
            const encoded = encodeAnswer(ok, value);
            const buffers = encoded.buffers.map((buffer) => buffer.slice());
            whenRuntimeReady(() => {
                engine.call("haylen_web_restore_screen", null, ["string", "string", "string", "number", "string", "buffers"], [record.plugin, record.screen, JSON.stringify(record.state), encoded.ok ? 1 : 0, encoded.json, buffers]);
                releaseViews(encoded.views);
            });
        };
        return { id: record.id, name: record.screen, token: record.token, resolve: (value) => restore(true, value), reject: (error) => restore(false, describeFailure(error)) };
    };

    // The web parts of plugins by id, each with the listeners of its context that hear app errors. Their ids are the plugins whose native part runs on the web.
    const plugins = new Map();

    haylen.nativePlugins = function () {
        return [...plugins.keys()];
    };

    // Native UI such as a full screen ad covers the app, which the engine then halts and mutes until the last cover ends.
    const coverApp = () => whenRuntimeReady(() => engine.call("haylen_web_cover_app", null, [], []));
    const uncoverApp = () => whenRuntimeReady(() => engine.call("haylen_web_uncover_app", null, [], []));

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

    // The safe area insets of the canvas box in page pixels, from the canvas pixels that `haylen.safeAreaInsets` reports as far as the insets reach into the box.
    const safeInsets = (box) => {
        const scale = box.width > 0 ? drawable.width / box.width : 1;
        const [left, top, right, bottom] = haylen.safeAreaInsets().map((value) => value / scale);
        return { left, top, right, bottom };
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
            whenRuntimeReady(() => engine.call("haylen_web_reserve_insets", null, ["string", "number", "number", "number", "number"], [key, insets.left, insets.top, insets.right, insets.bottom]));
        } else {
            whenRuntimeReady(() => engine.call("haylen_web_release_insets", null, ["string"], [key]));
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
        const scale = box.width > 0 ? drawable.width / box.width : 1;
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

    // The mirror of the UI for screen readers: a layer over the canvas that nothing shows, where every element of the UI is an element of the page with its ARIA role, name, value and states, placed where the app draws it, and two live regions that say the announcements of the app. Activating, focusing or adjusting an element of the mirror asks the engine for the same action through `haylen_web_accessibility_action`, and a focus that moves in the UI moves along in the mirror while the focus of the page is in it.
    haylen.accessibility = (function () {
        const states = { disabled: 1, checked: 2, selected: 4, focused: 8, expanded: 16, busy: 32 };
        const actions = { focus: 1, activate: 2, increment: 4, decrement: 8 };
        const roles = {
            group: "group",
            text: null,
            heading: "heading",
            image: "img",
            button: "button",
            checkBox: "checkbox",
            switch: "switch",
            radioGroup: "radiogroup",
            radioButton: "radio",
            tabList: "tablist",
            tab: "tab",
            slider: "slider",
            textField: "textbox",
            comboBox: "combobox",
            progressBar: "progressbar",
            list: "list",
            dialog: "dialog",
            alert: "alert",
            playArea: "application",
        };
        const mirror = { layer: null, polite: null, assertive: null, nodes: new Map(), elements: [], focused: 0 };

        const act = (id, action) => engine.call("haylen_web_accessibility_action", null, ["number", "number"], [id, action]);

        const createLayer = () => {
            const layer = document.createElement("div");
            layer.id = "haylen-accessibility";
            layer.style.cssText = "position:fixed;left:0;top:0;width:0;height:0;overflow:hidden;pointer-events:none;opacity:0;z-index:1;";
            for (const live of ["polite", "assertive"]) {
                const region = document.createElement("div");
                region.setAttribute("aria-live", live);
                region.setAttribute("aria-atomic", "true");
                region.style.cssText = "position:absolute;width:1px;height:1px;overflow:hidden;";
                layer.appendChild(region);
                mirror[live] = region;
            }

            Module.canvas.after(layer);
            mirror.layer = layer;
            const resizes = new ResizeObserver(place);
            resizes.observe(Module.canvas);
            window.addEventListener("resize", place);
            window.addEventListener("scroll", place, true);
            return layer;
        };

        const createNode = (element) => {
            const node = document.createElement("div");
            node.style.cssText = "position:absolute;margin:0;padding:0;overflow:hidden;";
            node.addEventListener("click", () => {
                if (node.haylenActions & actions.activate) {
                    act(node.haylenId, actions.activate);
                }
            });
            node.addEventListener("focus", () => {
                if (node.haylenActions & actions.focus && node.haylenId !== mirror.focused) {
                    act(node.haylenId, actions.focus);
                }
            });
            node.addEventListener("keydown", (event) => {
                const up = event.key === "ArrowUp" || event.key === "ArrowRight";
                const down = event.key === "ArrowDown" || event.key === "ArrowLeft";
                if ((up && node.haylenActions & actions.increment) || (down && node.haylenActions & actions.decrement)) {
                    act(node.haylenId, up ? actions.increment : actions.decrement);
                    event.preventDefault();
                    event.stopPropagation();
                } else if ((event.key === "Enter" || event.key === " ") && node.haylenActions & actions.activate) {
                    act(node.haylenId, actions.activate);
                    event.preventDefault();
                    event.stopPropagation();
                }
            });
            node.haylenId = element.id;
            return node;
        };

        const setAttribute = (node, name, value) => {
            if (value === null || value === undefined || value === "") {
                node.removeAttribute(name);
            } else if (node.getAttribute(name) !== String(value)) {
                node.setAttribute(name, String(value));
            }
        };

        // The roles of ARIA for the roles of the engine, where a text is plain text of the page and a text field holds its text.
        const describe = (node, element) => {
            const role = roles[element.role];
            setAttribute(node, "role", role);
            node.haylenActions = element.actions;
            const named = role !== null;
            setAttribute(node, "aria-label", named ? element.label : null);
            const text = named ? (element.role === "textField" ? element.value : "") : element.label;
            if (node.firstChild === null || node.firstChild.nodeType !== Node.TEXT_NODE) {
                node.insertBefore(document.createTextNode(text), node.firstChild);
            } else if (node.firstChild.data !== text) {
                node.firstChild.data = text;
            }

            setAttribute(node, "aria-level", element.role === "heading" ? 2 : null);
            setAttribute(node, "aria-valuetext", element.role === "slider" || element.role === "progressBar" ? element.value : null);
            setAttribute(node, "aria-description", element.hint);
            const checkable = element.role === "checkBox" || element.role === "switch" || element.role === "radioButton";
            setAttribute(node, "aria-checked", checkable ? String((element.states & states.checked) !== 0) : null);
            setAttribute(node, "aria-selected", element.role === "tab" ? String((element.states & states.selected) !== 0) : null);
            setAttribute(node, "aria-pressed", element.role === "button" && element.states & states.selected ? "true" : null);
            setAttribute(node, "aria-expanded", element.states & states.expanded ? "true" : null);
            setAttribute(node, "aria-disabled", element.states & states.disabled ? "true" : null);
            setAttribute(node, "aria-busy", element.states & states.busy ? "true" : null);
            setAttribute(node, "tabindex", element.actions & actions.focus ? -1 : null);
        };

        // Places every element in CSS pixels inside its parent, from its bounds in canvas pixels.
        const place = () => {
            const layer = mirror.layer;
            if (!layer) {
                return;
            }

            const box = Module.canvas.getBoundingClientRect();
            layer.style.left = box.left + "px";
            layer.style.top = box.top + "px";
            layer.style.width = box.width + "px";
            layer.style.height = box.height + "px";
            const scale = drawable.width > 0 ? box.width / drawable.width : 1;
            const byId = new Map(mirror.elements.map((element) => [element.id, element]));
            for (const element of mirror.elements) {
                const node = mirror.nodes.get(element.id);
                const parent = byId.get(element.parent);
                const originX = parent ? parent.bounds[0] : 0;
                const originY = parent ? parent.bounds[1] : 0;
                node.style.left = (element.bounds[0] - originX) * scale + "px";
                node.style.top = (element.bounds[1] - originY) * scale + "px";
                node.style.width = element.bounds[2] * scale + "px";
                node.style.height = element.bounds[3] * scale + "px";
            }
        };

        const update = (json) => {
            const tree = JSON.parse(json);
            const layer = mirror.layer || createLayer();
            const kept = new Map();
            for (const element of tree.elements) {
                const node = mirror.nodes.get(element.id) || createNode(element);
                describe(node, element);
                kept.set(element.id, node);
            }

            for (const [id, node] of mirror.nodes) {
                if (!kept.has(id)) {
                    node.remove();
                }
            }

            // Every element goes into its parent in the order screen readers read them.
            for (const element of tree.elements) {
                const parent = kept.get(element.parent) || layer;
                const node = kept.get(element.id);
                if (node.parentNode !== parent || node.nextSibling !== null) {
                    parent.appendChild(node);
                }
            }

            mirror.nodes = kept;
            mirror.elements = tree.elements;
            place();

            if (tree.focused !== mirror.focused) {
                mirror.focused = tree.focused;
                const node = kept.get(tree.focused);
                if (node && layer.contains(document.activeElement)) {
                    node.focus({ preventScroll: true });
                }
            }
        };

        // Clearing the region before the text makes screen readers say a text that repeats.
        const announce = (text, interrupt) => {
            if (!mirror.layer) {
                createLayer();
            }

            const region = interrupt ? mirror.assertive : mirror.polite;
            region.textContent = "";
            setTimeout(() => {
                region.textContent = text;
            }, 50);
        };

        return { update, announce };
    })();

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

    // The streams of the web parts of plugins by the key the page gives each, which the engine side opens once the runtime is ready and answers through `streamOpened` with the room of an audio stream, logging why the engine refused a stream.
    const streams = { open: new Map(), nextKey: 1 };

    haylen.streamOpened = function (key, opened, room) {
        const stream = streams.open.get(key);
        stream.opened = opened;
        stream.room = room;
    };

    // In the Worker mode the worker reports the room of each audio stream after each push and while the stream plays, and the page counts what it sent since.
    haylen.streamRoom = function (key, room) {
        streams.open.get(key).room = room;
    };

    // A video stream draws each source into a canvas of its size and hands the RGBA pixels to the engine, which keeps the newest frame for its texture. Pushes before the stream opened, and of a video without its first frame yet, are dropped and return `false`.
    const openVideoStream = (id, name) => {
        const stream = { key: streams.nextKey++, opened: false, room: 0, canvas: null, context: null };
        streams.open.set(stream.key, stream);
        whenRuntimeReady(() => engine.openStream(stream.key, "video", [id, name]));
        return {
            push(source, timestamp) {
                const [width, height] = sourceSize(source);

                if (!stream.opened || !(width > 0 && height > 0)) {
                    return false;
                }

                if (!stream.canvas || stream.canvas.width !== width || stream.canvas.height !== height) {
                    stream.canvas = new OffscreenCanvas(width, height);
                    stream.context = stream.canvas.getContext("2d", { willReadFrequently: true });
                }

                stream.context.clearRect(0, 0, width, height);
                stream.context.drawImage(source, 0, 0, width, height);
                const pixels = stream.context.getImageData(0, 0, width, height).data;
                return engine.pushVideo(stream.key, pixels, width, height, timestamp === undefined ? sourceTime(source) : timestamp);
            },
        };
    };

    // An audio stream hands interleaved `Float32Array` samples to the ring of the engine, and `push` returns how many frames fit. The capacity defaults to one second of frames.
    const openAudioStream = (id, name, options) => {
        const { sampleRate, channels, capacity } = { capacity: options.sampleRate, ...options };
        const stream = { key: streams.nextKey++, opened: false, room: 0 };
        streams.open.set(stream.key, stream);
        whenRuntimeReady(() => engine.openStream(stream.key, "audio", [id, name, sampleRate, channels, capacity]));
        return {
            push(samples) {
                if (samples.length % channels !== 0) {
                    throw new Error("Audio samples come in whole frames, one sample for every channel.");
                }

                if (!stream.opened) {
                    return 0;
                }

                return engine.pushAudio(stream, samples, channels);
            },
        };
    };

    // What a plugin needs from the page: a secure context, an API of the browser, which is a path of properties from the global object such as `navigator.contacts`, and a feature that the permissions policy of the page allows. Each missing requirement logs once per plugin, and none of them has a file of a project that adds it, so `file` stays empty, while `snippet` holds the text that allows a feature of the permissions policy.
    const requirements = { kinds: ["secureContext", "api", "permissionsPolicy"], reported: new Set() };

    const hasApi = (path) => path.split(".").reduce((value, part) => (value === undefined || value === null ? undefined : value[part]), globalThis) != null;

    // Browsers that do not tell the policy of the page, such as Firefox and Safari, and a policy that does not govern the feature, which then allows it nowhere, leave the error of the API to tell instead.
    const allowsFeature = (feature) => {
        const policy = document.permissionsPolicy || document.featurePolicy;
        return !policy || !policy.features().includes(feature) || policy.allowsFeature(feature);
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

        const failure = findMissing("The plugin \"" + id + "\"", id, needs);
        if (failure) {
            throw Object.assign(new Error(failure.message), { code: "unsupported", data: { missing: failure.missing } });
        }
    };

    // Returns what the page lacks of the needs of an owner, as the sentence that names it and the list of `{kind, name, file, snippet}`, after it logged each missing one once, or nothing when the page holds everything.
    const findMissing = (owner, key, needs) => {
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
            return null;
        }

        for (const requirement of missing) {
            const reported = key + "\n" + requirement.entry.kind + "\n" + requirement.entry.name;
            if (!requirements.reported.has(reported)) {
                requirements.reported.add(reported);
                const message = owner + " needs " + requirement.summary + ", which the page lacks, so the calls that need it fail with the code \"unsupported\". " + requirement.instructions;
                console.warn(message);
                notify("onLog", "warning", message);
            }
        }

        const summaries = missing.map((requirement) => requirement.summary);
        const list = summaries.length === 1 ? summaries[0] : summaries.slice(0, -1).join(", ") + " and " + summaries[summaries.length - 1];
        return { message: owner + " needs " + list + ", which the page lacks.", missing: missing.map((requirement) => requirement.entry) };
    };

    // The permissions of the engine in `system.permission`. Each one names the capability in words, what the page needs for it, the name of the Permissions API that tells its state, where the browser knows it, and how the page asks for it, which answers `{state, message, missing}`. A name that the Permissions API does not know answers the state of `unnamed`. The permissions of plugins take the same shape through `context.registerPermission`.
    const permissions = {
        localNetwork: { capability: "the local network", query: "local-network-access", unnamed: "granted", request: () => requestLocalNetwork() },
    };

    // The sentence of a use of a capability that a request did not allow, the same as the one of the engine: after `prompt`, where the browser did not show its prompt, it says that the app may ask again later, and after `denied` it names how to ask for it again.
    const permissionRefusal = (permission, name, answered) => {
        if (answered === "prompt") {
            return "The system did not show its prompt for " + permission.capability + ", so the app does not hold it yet. Ask for it again later with \"system.requestPermission('" + name + "')\".";
        }

        return "The person has not allowed " + permission.capability + ". Ask for it with \"system.requestPermission('" + name + "')\", which shows the prompt of the system while the system still asks, or the person allows it in the settings of the system.";
    };

    // Answers the state of a name of the Permissions API, or nothing for a name that the browser does not know.
    const queryPermission = async (name) => {
        if (!navigator.permissions || typeof navigator.permissions.query !== "function") {
            return null;
        }

        try {
            return (await navigator.permissions.query({ name })).state;
        } catch {
            return null;
        }
    };

    // A capability that the browser lacks, or that the page lacks a requirement of, answers `unsupported` before anything asks.
    const blockedPermission = (permission, owner, key) => {
        const missing = permission.needs ? findMissing(owner, key, permission.needs) : null;
        if (missing) {
            return { state: "unsupported", message: missing.message, missing: missing.missing };
        }

        const unavailable = permission.unavailable ? permission.unavailable() : null;
        return unavailable ? { state: "unsupported", message: unavailable } : null;
    };

    const checkPermission = async (permission, owner = "The engine", key = "engine") => {
        const blocked = blockedPermission(permission, owner, key);
        if (blocked) {
            return blocked;
        }

        if (permission.check) {
            return permission.check();
        }

        return { state: (await queryPermission(permission.query)) || permission.unnamed };
    };

    const requestPermission = async (permission, owner = "The engine", key = "engine") => {
        const blocked = blockedPermission(permission, owner, key);
        return blocked || permission.request();
    };

    // A permission of a plugin shows the prompt of the browser only while its state is `prompt`, or `unknown` where the browser tells nothing before it asks, and answers any other state without asking.
    const requestPluginPermission = async (permission, owner, key) => {
        const current = await checkPermission(permission, owner, key);
        return current.state === "prompt" || current.state === "unknown" ? permission.request() : current;
    };

    // Chromium asks for the local network at the first access to it, which a page cannot ask for before, so a state that would prompt answers unknown.
    const requestLocalNetwork = async () => {
        const answer = await checkPermission(permissions.localNetwork);
        return answer.state === "prompt" ? { state: "unknown" } : answer;
    };

    // In the Worker mode the state of the page reaches the worker before the answer, so the app that hears the answer reads the state that the call left, such as fullscreen.
    const answerGatedCall = (id, answer) => {
        whenRuntimeReady(() => {
            if (hostedInWorker) {
                worker.postLayout();
            }

            engine.call("haylen_web_answer_gated_call", null, ["number", "string"], [id, JSON.stringify(answer)]);
        });
    };

    // A failure of the code of a check or a request answers `unsupported` with its message, so the engine always hears back.
    const settleGatedCall = (id, work) => {
        Promise.resolve().then(work).then((answer) => answerGatedCall(id, answer), (error) => answerGatedCall(id, { state: "unsupported", message: "The browser failed to answer: " + error.message }));
    };

    // The browser lists every screen through the Window Management API once the person allows it, which it asks for only shortly after a click or a key of the person.
    haylen.requestEveryScreen = function (id) {
        settleGatedCall(id, async () => {
            const missing = findMissing("Every screen", "screens", { secureContext: true, permissionsPolicy: "window-management" });
            if (missing) {
                return { state: "unsupported", message: missing.message, missing: missing.missing };
            }

            if (typeof window.getScreenDetails !== "function") {
                return { state: "unsupported", message: "The browser cannot list every screen, since it lacks the Window Management API." };
            }

            try {
                await seeEveryScreen();
                return { state: "granted" };
            } catch (error) {
                if (error.name === "NotAllowedError") {
                    return { state: "denied", message: "The person has not allowed the page to see every screen, which the settings of the site in the browser allow again." };
                }

                return { state: "unsupported", message: "The browser cannot list every screen: " + error.message };
            }
        });
    };

    haylen.checkPermission = function (id, kind) {
        settleGatedCall(id, () => checkPermission(permissions[kind]));
    };

    haylen.requestPermission = function (id, kind) {
        settleGatedCall(id, () => requestPermission(permissions[kind]));
    };

    // An answer of the code of a plugin is the name of a state, or `{state, message}` for `unsupported` with the reason.
    const readPluginAnswer = (value) => {
        const answer = typeof value === "string" ? { state: value } : value;
        if (!answer || !["granted", "prompt", "denied", "unknown", "unsupported"].includes(answer.state)) {
            throw new TypeError("A permission of a plugin answers \"granted\", \"prompt\", \"denied\", \"unknown\" or \"unsupported\", or {state, message}.");
        }

        return { state: answer.state, message: answer.message || "" };
    };

    // Turns what `context.registerPermission` takes into the shape of the permissions of the engine, after it checked every key.
    const createPluginPermission = (id, spec) => {
        const keys = ["name", "capability", "needs", "query", "state", "request"];
        for (const key of Object.keys(spec || {})) {
            if (!keys.includes(key)) {
                throw new TypeError("A permission of the plugin \"" + id + "\" takes \"" + keys.join("\", \"") + "\", not \"" + key + "\".");
            }
        }

        if (!spec || typeof spec.name !== "string" || spec.name === "" || typeof spec.capability !== "string" || typeof spec.request !== "function") {
            throw new TypeError("A permission of the plugin \"" + id + "\" needs a \"name\", a \"capability\" in words and a \"request\" function.");
        }

        if (spec.state !== undefined && typeof spec.state !== "function") {
            throw new TypeError("The \"state\" of the permission \"" + spec.name + "\" of the plugin \"" + id + "\" is a function.");
        }

        if (spec.state === undefined && typeof spec.query !== "string") {
            throw new TypeError("The permission \"" + spec.name + "\" of the plugin \"" + id + "\" tells its state through a \"query\" of the Permissions API or a \"state\" function.");
        }

        const needs = spec.needs || {};
        for (const key of Object.keys(needs)) {
            if (!requirements.kinds.includes(key)) {
                throw new TypeError("A permission requires \"secureContext\", \"api\" or \"permissionsPolicy\", not \"" + key + "\".");
            }
        }

        return {
            capability: spec.capability,
            needs,
            query: spec.query,
            unnamed: "unknown",
            check: spec.state ? async () => readPluginAnswer(await spec.state()) : undefined,
            request: async () => readPluginAnswer(await spec.request()),
        };
    };

    // The handlers `<id>.permission` and `<id>.requestPermission` answer the permissions that the plugin registered, and a name that it did not register on the web answers `unsupported`.
    const answerPluginPermission = async (id, plugin, params, asking) => {
        const name = params && params.name;
        const permission = plugin.permissions.get(name);
        if (!permission) {
            return { state: "unsupported", message: "The plugin \"" + id + "\" registers no permission \"" + name + "\" on the web." };
        }

        const owner = "The plugin \"" + id + "\"";
        return asking ? requestPluginPermission(permission, owner, id) : checkPermission(permission, owner, id);
    };

    const registerPluginPermission = (id, plugin, spec) => {
        const permission = createPluginPermission(id, spec);

        if (plugin.permissions.has(spec.name)) {
            throw new Error("The plugin \"" + id + "\" registered the permission \"" + spec.name + "\" twice.");
        }

        if (plugin.permissions.size === 0) {
            haylen.register(id + ".permission", (params) => answerPluginPermission(id, plugin, params, false));
            haylen.register(id + ".requestPermission", (params) => answerPluginPermission(id, plugin, params, true));
        }

        plugin.permissions.set(spec.name, permission);
    };

    // Asks for the permission where the page does not hold it, and fails with the code `denied` when the person refused it or the browser did not show its prompt, or `unsupported` with what the page lacks or for a name that the plugin did not register, as plugins expect before they use a capability. A state that the browser tells nothing of lets the use try.
    const requirePermission = async (id, plugin, name) => {
        const permission = plugin.permissions.get(name);
        if (!permission) {
            throw Object.assign(new Error("The plugin \"" + id + "\" requires the permission \"" + name + "\", which it did not register with \"registerPermission\"."), { code: "unsupported" });
        }

        const answer = await requestPluginPermission(permission, "The plugin \"" + id + "\"", id);
        if (answer.state === "denied" || answer.state === "prompt") {
            throw Object.assign(new Error(permissionRefusal(permission, name, answer.state)), { code: "denied" });
        }

        if (answer.state === "unsupported") {
            throw Object.assign(new Error(answer.message), { code: "unsupported", data: answer.missing ? { missing: answer.missing } : undefined });
        }
    };

    // Makes the context that the web module of a plugin receives in `load(context)`. Methods and events take the id of the plugin in front of their names, as the Lua handle of the plugin expects. The loader calls it for every plugin before the runtime starts.
    haylen.createPluginContext = function (id, config) {
        if (plugins.has(id)) {
            throw new Error("The plugin \"" + id + "\" already has a context.");
        }

        const plugin = { errorListeners: [], permissions: new Map() };
        plugins.set(id, plugin);

        if (hostedInWorker) {
            worker.setState({ nativePlugins: haylen.nativePlugins() });
        }

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
            openLink(url) {
                haylen.openLink(url);
            },
            videoStream(name) {
                return openVideoStream(id, name);
            },
            audioStream(name, options) {
                return openAudioStream(id, name, options);
            },
            // The class of the data views of Varn, which share memory of the heap with the app, so a plugin makes views with "VarnDataView.allocate" once the runtime started.
            VarnDataView: VarnView,
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
            registerPermission(spec) {
                registerPluginPermission(id, plugin, spec);
            },
            requirePermission(name) {
                return requirePermission(id, plugin, name);
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
                    releaseViews(entry.views);
                }
            });
        }

        notify("onStarted", app);
    };

    haylen.reportStopped = function () {
        notify("onStopped");
    };

    // The splash of the page ends once the app drew its first frame and the splash of `haylen.json` lasted its duration, fading out over the seconds it gives.
    haylen.reportSplashEnded = function (fadeOut) {
        notify("onSplashEnded", { fadeOut });
    };

    // Reports of hot reload arrive after every batch of changes that applied, as `{restarted: false, mode, modules, restored, assets, resumed, milliseconds}`, or as `{restarted: true, reason}` before a restart.
    haylen.reportReloaded = function (report) {
        notify("onReloaded", report);
    };

    // Statistics arrive about once per second while the app runs: fps, frame times, renderer counters, cached assets, audio voices, profiler scopes and the audio output.
    haylen.reportStats = function (stats) {
        stats.audio = audio.stats();
        notify("onStats", stats);
    };

    // The downloads of the module and of the package that the engine side follows.
    haylen.reportLoadProgress = function (progress) {
        notify("onLoadProgress", progress);
    };

    haylen.canvasSelector = function () {
        if (!(Module.canvas instanceof HTMLCanvasElement) || !Module.canvas.id) {
            throw new Error("The property \"Module.canvas\" must be a canvas element with an id.");
        }

        return "#" + CSS.escape(Module.canvas.id);
    };

    // The safe area and the fold change only with the layout of the page, so the engine, which asks for them in every frame, reads them once after each change of the size, the scroll or the posture of the page.
    const pageLayout = { safeArea: null, fold: undefined };
    const forgetLayout = () => {
        pageLayout.safeArea = null;
        pageLayout.fold = undefined;
    };

    // Takes the width in pixels of the drawable of the engine, which turns framebuffer pixels into page pixels over the canvas box. The canvas takes every pixel of the screen, while the pixel density cap of the app may keep the drawable narrower.
    haylen.setDrawableWidth = function (width) {
        drawable.width = width;
        forgetLayout();

        if (hostedInWorker) {
            worker.postLayout();
        }
    };

    window.addEventListener("resize", forgetLayout);
    window.addEventListener("scroll", forgetLayout, { passive: true });

    if (navigator.devicePosture) {
        navigator.devicePosture.addEventListener("change", forgetLayout);
    }

    // Reads `env(safe-area-inset-*)` of the viewport through a hidden probe and returns the part of each inset that covers the canvas, in framebuffer pixels, so a canvas away from the edges that a cutout or the bars of the system cover has none.
    haylen.safeAreaInsets = function () {
        if (pageLayout.safeArea) {
            return pageLayout.safeArea;
        }

        let probe = document.getElementById("haylen-safe-area");
        if (!probe) {
            probe = document.createElement("div");
            probe.id = "haylen-safe-area";
            probe.style.cssText = "position:fixed;visibility:hidden;pointer-events:none;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);";
            document.body.appendChild(probe);
        }

        const style = getComputedStyle(probe);
        const box = Module.canvas.getBoundingClientRect();
        const scale = box.width > 0 ? drawable.width / box.width : 0;
        const covered = [
            parseFloat(style.paddingLeft) - box.left,
            parseFloat(style.paddingTop) - box.top,
            parseFloat(style.paddingRight) - (window.innerWidth - box.right),
            parseFloat(style.paddingBottom) - (window.innerHeight - box.bottom),
        ];
        pageLayout.safeArea = covered.map((inset) => Math.max(0, inset || 0) * scale);
        return pageLayout.safeArea;
    };

    // Reads the segments of the viewport, which a browser on a foldable or dual-screen device splits at the fold, and the posture of the device, and returns the fold across the canvas in framebuffer pixels as `[left, top, right, bottom, vertical, halfOpened, separating, occluding]`, or `null` while the viewport has one segment.
    haylen.fold = function () {
        if (pageLayout.fold === undefined) {
            pageLayout.fold = readFold();
        }

        return pageLayout.fold;
    };

    const readFold = () => {
        const segments = window.viewport && window.viewport.segments;
        if (!segments || segments.length < 2) {
            return null;
        }

        const [first, second] = segments;
        const vertical = second.left >= first.right;
        const box = Module.canvas.getBoundingClientRect();
        const scale = box.width > 0 ? drawable.width / box.width : 0;
        const gap = vertical ? [first.right, box.top, second.left, box.bottom] : [box.left, first.bottom, box.right, second.top];
        const bounds = [(gap[0] - box.left) * scale, (gap[1] - box.top) * scale, (gap[2] - box.left) * scale, (gap[3] - box.top) * scale];
        const folded = navigator.devicePosture !== undefined && navigator.devicePosture.type === "folded";
        const occluding = vertical ? gap[2] > gap[0] : gap[3] > gap[1];
        return [...bounds, vertical ? 1 : 0, folded ? 1 : 0, 1, occluding ? 1 : 0];
    };

    // The screens of the page: the one it shows on until the person lets the page see every screen through the Window Management API, whose screens then report their changes. Until then `screen.isExtended` tells whether the device has more screens, which the page reports as unlisted. Each screen gets an id the first time the page sees it, which it keeps while it stays connected, and its points are the CSS pixels of the screens around it.
    const pageScreens = { details: null, ids: new WeakMap(), counted: 0 };
    const screenId = (shown) => {
        if (!pageScreens.ids.has(shown)) {
            pageScreens.counted += 1;
            pageScreens.ids.set(shown, "screen-" + pageScreens.counted);
        }

        return pageScreens.ids.get(shown);
    };

    const describeScreen = (shown) => {
        const detailed = pageScreens.details !== null;
        return {
            id: screenId(shown),
            name: detailed ? shown.label : "",
            bounds: detailed ? [shown.left, shown.top, shown.width, shown.height] : [0, 0, shown.width, shown.height],
            workArea: detailed ? [shown.availLeft, shown.availTop, shown.availWidth, shown.availHeight] : [0, 0, shown.availWidth, shown.availHeight],
            scale: detailed ? shown.devicePixelRatio : window.devicePixelRatio,
            primary: detailed ? shown.isPrimary : true,
        };
    };

    const displaysChanged = () => {
        if (hostedInWorker) {
            worker.setState({ displays: haylen.displays() });
        }

        whenRuntimeReady(() => engine.call("haylen_web_displays_changed", null, [], []));
    };

    haylen.displays = function () {
        const all = pageScreens.details !== null ? pageScreens.details.screens : [window.screen];
        const current = pageScreens.details !== null ? pageScreens.details.currentScreen : window.screen;
        const unlisted = pageScreens.details === null && window.screen.isExtended === true;
        return JSON.stringify({ current: screenId(current), displays: all.map(describeScreen), unlisted });
    };

    // Asks to see every screen, and once the page sees them follows their changes, which `requestEveryScreen` does for the app.
    const seeEveryScreen = () => window.getScreenDetails().then((details) => {
        if (pageScreens.details === null) {
            // The screen of the page keeps its id as the browser describes it in detail, so the app sees the others join it rather than a new screen.
            pageScreens.ids.set(details.currentScreen, screenId(window.screen));
            pageScreens.details = details;
            details.addEventListener("screenschange", displaysChanged);
            details.addEventListener("currentscreenchange", displaysChanged);
            displaysChanged();
        }
    });

    if (window.screen.addEventListener) {
        window.screen.addEventListener("change", displaysChanged);
    }

    // A page that already holds the permission, which the person gave on an earlier visit or in the settings of the site, sees every screen without asking again, from the start and from the moment the permission changes to granted.
    const followScreensPermission = async () => {
        if (typeof window.getScreenDetails !== "function") {
            return;
        }

        const status = await navigator.permissions.query({ name: "window-management" });
        const follow = () => {
            if (status.state === "granted") {
                seeEveryScreen().catch((error) => console.warn("The page holds the permission to see every screen, but the browser did not describe them: " + error.message));
            }
        };
        status.addEventListener("change", follow);
        follow();
    };

    followScreensPermission().catch((error) => console.warn("The browser did not tell whether the page may see every screen: " + error.message));

    // The outputs of the app on other screens, each a window of the browser that `window.open` places on its screen through the Window Management API, with a canvas that fills the window and that the WebGPU device presents into. A browser opens such a window only shortly after a click or a key of the person, so the app opens an output from a button, and a window that the browser blocks ends the output with the reason. The window covers the work area of its screen when the output asks for fullscreen, since a page cannot make another window fullscreen without a gesture inside it. The endings are those of `platform::Output::Ending`, in its order.
    const outputEndings = { person: 1, disconnected: 2, unsupported: 3 };
    const pageOutputs = new Map();
    const endOutput = (number, ending, reason) => {
        const output = pageOutputs.get(number);
        if (output !== undefined) {
            pageOutputs.delete(number);
            output.window.close();
        }

        whenRuntimeReady(() => engine.call("haylen_web_output_ended", null, ["number", "number", "string"], [number, ending, reason]));
    };

    haylen.outputCanvas = (number) => pageOutputs.get(number)?.canvas ?? null;
    haylen.openOutput = function (number, display, title, fullscreen, width, height) {
        const target = pageScreens.details !== null ? pageScreens.details.screens.find((shown) => screenId(shown) === display) : undefined;
        if (target === undefined) {
            queueMicrotask(() => endOutput(number, outputEndings.disconnected, ""));
            return;
        }

        const size = fullscreen ? [target.availWidth, target.availHeight] : [Math.min(width, target.availWidth), Math.min(height, target.availHeight)];
        const left = target.availLeft + Math.round((target.availWidth - size[0]) / 2);
        const top = target.availTop + Math.round((target.availHeight - size[1]) / 2);
        // Every output opens a window of its own, so a window that an earlier output or the popup blocker of the browser left open never stands in for it.
        const opened = window.open("", "_blank", `popup,left=${left},top=${top},width=${size[0]},height=${size[1]}`);
        if (opened === null) {
            queueMicrotask(() => endOutput(number, outputEndings.unsupported, "The browser blocked the window of the output. A browser opens a window only shortly after a click or a key of the person, so open the output from a button."));
            return;
        }

        // A window that the page cannot fill ends the output with the reason of the browser, which the log of the engine shows, instead of staying empty.
        try {
            fillOutputWindow(number, opened, title);
        } catch (error) {
            opened.close();
            queueMicrotask(() => endOutput(number, outputEndings.unsupported, "The window of the output could not be prepared: " + error.message));
        }
    };

    // Lays a canvas over the whole window of an output, which the WebGPU device presents into, follows its size and hands the engine its pointer.
    const fillOutputWindow = (number, opened, title) => {
        const page = opened.document;
        page.title = title;
        page.body.style.cssText = "margin:0;overflow:hidden;background:#000";
        const canvas = page.createElement("canvas");
        canvas.style.cssText = "display:block;width:100vw;height:100vh;touch-action:none";
        page.body.appendChild(canvas);
        const output = { window: opened, canvas: canvas };
        pageOutputs.set(number, output);

        // In the Worker mode the worker draws into the canvas of the output, which the page hands it before it reports its size.
        if (hostedInWorker) {
            const offscreen = canvas.transferControlToOffscreen();
            worker.post({ type: "output", number, canvas: offscreen }, [offscreen]);
        }

        const reportSize = () => {
            const scale = opened.devicePixelRatio;
            const pixels = [Math.max(1, Math.round(canvas.clientWidth * scale)), Math.max(1, Math.round(canvas.clientHeight * scale))];
            whenRuntimeReady(() => engine.call("haylen_web_output_canvas", null, ["number", "number", "number", "number"], [number, pixels[0], pixels[1], scale]));
        };
        const sendPointer = (kind, event) => {
            const values = [number, kind, event.offsetX, event.offsetY, event.button ?? 0, 0, 0];
            whenRuntimeReady(() => engine.call("haylen_web_output_pointer", null, ["number", "number", "number", "number", "number", "number", "number"], values));
        };
        canvas.addEventListener("pointerdown", (event) => sendPointer(0, event));
        canvas.addEventListener("pointerup", (event) => sendPointer(1, event));
        canvas.addEventListener("pointermove", (event) => sendPointer(2, event));
        canvas.addEventListener("wheel", (event) => {
            event.preventDefault();
            whenRuntimeReady(() => engine.call("haylen_web_output_pointer", null, ["number", "number", "number", "number", "number", "number", "number"], [number, 3, event.offsetX, event.offsetY, 0, -event.deltaX / 100, -event.deltaY / 100]));
        }, { passive: false });
        opened.addEventListener("resize", reportSize);
        opened.addEventListener("pagehide", () => {
            if (pageOutputs.get(number) === output) {
                pageOutputs.delete(number);
                endOutput(number, outputEndings.person, "");
            }
        });
        reportSize();
    };

    haylen.closeOutput = function (number) {
        const output = pageOutputs.get(number);
        if (output !== undefined) {
            pageOutputs.delete(number);
            output.window.close();
        }
    };

    // Native text editing. A hidden textarea, or a password input, lies over the focused field next to the canvas, so phones open their keyboard and browsers bring their input methods, paste, copy and undo. The engine also lists the fields on screen, because Safari on iOS opens the keyboard only when an element takes the focus inside a user gesture.
    const text = { elements: {}, field: null, fields: [], tapSlop: 0, active: null, composition: null, finishing: false, keyboard: [0, 0, 0, 0] };
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
        const scale = box.width / drawable.width;
        return { left: box.left + bounds[0] * scale, top: box.top + bounds[1] * scale, width: bounds[2] * scale, height: bounds[3] * scale };
    };

    // The limit of a field counts code points, as on every platform, so the page cuts the text itself instead of with "maxlength", which counts UTF-16 units. A composition is cut once it ends.
    const limit = (target, field) => {
        const value = target.value;
        if (field.maxLength <= 0 || text.composition || toCodePoints(value, value.length) <= field.maxLength) {
            return;
        }

        const cut = toUtf16(value, field.maxLength);
        const caret = Math.min(target.selectionEnd, cut);
        target.value = value.slice(0, cut);
        target.setSelectionRange(caret, caret);
    };

    const report = () => {
        const target = text.active;
        const field = text.field;
        if (!field || !target) {
            return;
        }

        limit(target, field);
        const value = target.value;
        const composition = text.composition;
        const start = composition ? toCodePoints(value, composition.start) : -1;
        const end = composition ? toCodePoints(value, Math.min(value.length, composition.start + composition.length)) : -1;
        engine.call("haylen_web_text_edited", null, ["number", "number", "string", "number", "number", "number", "number"], [field.id, field.revision, value, toCodePoints(value, target.selectionStart), toCodePoints(value, target.selectionEnd), start, end]);
    };

    // Actions are 0 for submit, 1 for next, 2 for cancel and 3 for a keyboard the user closed.
    const act = (action) => {
        if (text.field) {
            engine.call("haylen_web_text_action", null, ["number", "number"], [text.field.id, action]);
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
                const scale = drawable.width / box.width;
                frame = [0, (top - box.top) * scale, drawable.width, (box.bottom - top) * scale];
            }
        }

        if (frame.some((value, index) => value !== text.keyboard[index])) {
            text.keyboard = frame;
            engine.call("haylen_web_keyboard", null, ["number", "number", "number", "number"], frame);
        }
    };

    const isElement = (target) => target === text.elements.textarea || target === text.elements.password;

    // Other elements of the page own the keys they receive, such as the buttons of a plugin dialog or the fields of an editor next to the canvas, while the app owns the keys of the canvas, the body and the page.
    const isForeign = (target) => target instanceof Element && target !== Module.canvas && target !== document.body && target !== document.documentElement && !isElement(target);

    // The keys whose press reached the app, whose release reaches it too wherever the focus went in between, such as to the field that the press started editing, so no key stays held.
    const held = new Set();

    // A key that reaches the app goes on to SDL on the page, and to the worker in the Worker mode, whose page reads no key otherwise.
    const reachApp = (event) => {
        if (hostedInWorker) {
            worker.forwardKey(event);
        }
    };

    // SDL listens for keys on the window once they leave the element that has the focus. The keys of other elements of the page reach those elements and never the app, which SDL skips by their time stamp. A field of the UI owns every key, so SDL never applies them twice, and its return, tab and escape become actions. The plain keyboard of `haylen.window.setKeyboardVisible` leaves keys to SDL and only takes their text.
    const onKey = (event) => {
        if (event.type === "keyup" && held.delete(event.code)) {
            reachApp(event);
            return;
        }

        if (isForeign(event.target)) {
            Module.haylenIgnoredKey = event.timeStamp;
            return;
        }

        const field = text.field;
        if (!field || !isElement(event.target) || (field.id === 0 && event.type !== "keypress")) {
            if (event.type === "keydown") {
                held.add(event.code);
            }

            reachApp(event);
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

    // The focus moving to or from an element, such as a field, the canvas or the button of a plugin dialog, is no focus change of the page, so SDL reports only the focus of the window. The engine releases every held key when the window loses the focus, so the page forgets them then too. A blur of a field while the page keeps the focus means the user closed the keyboard or left the field, unless the engine ended the editing.
    const onFocus = (event) => {
        if (event.target === window) {
            if (event.type === "blur") {
                held.clear();
            }

            return;
        }

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

    // These listeners are added when the page loads, before SDL adds its own, so they run first.
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

    text.setFields = function (fields, tapSlop) {
        text.fields = fields;
        text.tapSlop = tapSlop;
    };

    // The canvas takes the focus on a press, as an element with a tab index, but the engine decides when the editing of a field ends. So a press never takes the focus from the element that edits, such as the press that browsers send after a tap, which would close the keyboard that the tap opened.
    const keepEditingFocus = (event) => {
        if (isElement(document.activeElement)) {
            event.preventDefault();
        }
    };

    // The presses on the canvas by pointer, with where each one started and whether another pointer pressed meanwhile.
    const presses = new Map();

    const notePress = (event) => {
        for (const press of presses.values()) {
            press.shared = true;
        }

        presses.set(event.pointerId, { x: event.clientX, y: event.clientY, shared: presses.size > 0 });
    };

    const isInside = (place, x, y) => x >= place.left && x <= place.left + place.width && y >= place.top && y <= place.top + place.height;

    // A tap on a field focuses its element inside the gesture, before the engine even sees the tap, as the engine taps it: a press that starts and ends on the field, travels less than the slop of the engine and shares the screen with no other pointer. Phones open their keyboard only for a focus inside a gesture, so an element that already has the focus, such as the one of a field that the app focused as its screen opened, takes it again.
    const focusTapped = (event) => {
        const press = presses.get(event.pointerId);
        presses.delete(event.pointerId);
        if (!press || press.shared || Math.hypot(event.clientX - press.x, event.clientY - press.y) >= toPage([0, 0, text.tapSlop, 0]).width) {
            return;
        }

        for (const field of text.fields) {
            const place = toPage(field.bounds);
            if (isInside(place, press.x, press.y) && isInside(place, event.clientX, event.clientY)) {
                const target = element(field.password);
                configure(target, field);

                if (document.activeElement === target) {
                    text.finishing = true;
                    target.blur();
                    text.finishing = false;
                }

                target.focus({ preventScroll: true });

                return;
            }
        }
    };

    // The audio outputs of the engine, one per audio device, and their contexts. The engine side mixes blocks of interleaved samples, on the page or in the worker of the Worker mode, and posts them through a `MessagePort` to an `AudioWorkletNode`, whose processor in `haylen-audio-worklet.js` plays them from a ring buffer. The page owns the context and the node and hands the processor one end of a `MessageChannel` and the mixer the other, so in the Worker mode the blocks travel from the worker to the processor without passing through the page.
    const audio = { outputs: new Map(), contexts: new Map() };
    haylen.audio = audio;

    // The videos of the engine, which `WebVideoDecoder.cpp` drives: each plays in a video element that nothing shows, made from the bytes of its file. The element keeps the clock, the engine side copies its newest picture into wasm memory, and its sound leaves the element through a `MediaElementAudioSourceNode` of the context of the audio output into the processor `haylen-video-tap` of `haylen-audio-worklet.js`, which hands the samples back for the mixer of the engine. A browser that refuses to play sound before the user interacted with the page plays the video muted until the next tap, click or key. In the Worker mode the page posts each new picture of a video to the worker as an `ImageBitmap`, with what the element knows and its sound.
    haylen.video = (function () {
        const videos = new Map();

        // The sound joins the context of the audio output of the engine, or the video plays without sound while the app has none.
        const connectSound = (video) => {
            const [output] = audio.outputs.values();

            if (!output || !output.node) {
                return;
            }

            const context = output.shared.context;
            const source = context.createMediaElementSource(video.element);
            const tap = new AudioWorkletNode(context, "haylen-video-tap", { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2], channelCount: 2, channelCountMode: "explicit" });
            const silence = context.createGain();
            silence.gain.value = 0;
            tap.port.onmessage = (event) => (hostedInWorker ? worker.post({ type: "videoSound", id: video.id, samples: event.data }, [event.data.buffer]) : video.samples.push(event.data));
            source.connect(tap);
            tap.connect(silence);
            silence.connect(context.destination);
            video.tap = { source, tap, silence };
        };

        // What the element knows, with a width of 0 until the metadata arrived.
        const describe = (video) => {
            const element = video.element;
            const ready = element.readyState >= HTMLMediaElement.HAVE_METADATA;
            const [output] = audio.outputs.values();
            return { width: ready ? element.videoWidth : 0, height: ready ? element.videoHeight : 0, duration: ready && Number.isFinite(element.duration) ? element.duration : 0, sampleRate: output ? output.shared.context.sampleRate : 0, error: video.error, ended: element.ended, time: element.currentTime };
        };

        // A picture is new once the time of the element moved since the last one that the worker took, and the page posts one at a time, so a slow worker gets the newest picture rather than a queue of them.
        const follow = (video) => {
            if (!videos.has(video.id)) {
                return;
            }

            const element = video.element;
            const state = describe(video);
            if (JSON.stringify(state) !== video.posted) {
                video.posted = JSON.stringify(state);
                worker.post({ type: "videoState", id: video.id, state });
            }

            if (!video.capturing && element.readyState >= HTMLMediaElement.HAVE_CURRENT_DATA && element.currentTime !== video.sentTime && element.videoWidth > 0) {
                const time = element.currentTime;
                video.capturing = true;
                createImageBitmap(element).then((bitmap) => {
                    video.capturing = false;
                    video.sentTime = time;

                    if (!videos.has(video.id)) {
                        bitmap.close();
                        return;
                    }

                    worker.post({ type: "videoFrame", id: video.id, bitmap, time }, [bitmap]);
                }, (error) => {
                    video.capturing = false;
                    video.error = "The browser could not capture a picture of the video: " + error.message;
                });
            }

            requestAnimationFrame(() => follow(video));
        };

        const open = (id, type, parts) => {
            const video = { id, url: URL.createObjectURL(new Blob(parts, { type })), element: null, canvas: null, context: null, shownTime: -1, error: "", tap: null, samples: [], waitingForUser: false, capturing: false, sentTime: -1, posted: "" };
            videos.set(id, video);
            const element = document.createElement("video");
            element.playsInline = true;
            element.preload = "auto";
            element.addEventListener("error", () => {
                video.error = element.error ? element.error.message || "The browser could not play the video." : "The browser could not play the video.";
            });
            element.src = video.url;
            video.element = element;
            connectSound(video);

            if (hostedInWorker) {
                follow(video);
            }
        };

        const info = (id) => describe(videos.get(id));

        const control = (id, playing, rate) => {
            const video = videos.get(id);
            video.element.playbackRate = rate;
            if (!playing) {
                video.element.pause();
                return;
            }

            video.element.play().catch((error) => {
                if (error.name !== "NotAllowedError") {
                    return;
                }

                video.element.muted = true;
                video.waitingForUser = true;
                video.element.play().catch(() => {});
            });
        };

        // Copies the newest picture into `pixels`, the RGBA bytes of `width` by `height` in wasm memory. The answer is 0 while no new picture shows, 1 for a picture, 2 at the end and 3 for an error.
        const frame = (id, pixels, width, height) => {
            const video = videos.get(id);
            const element = video.element;

            if (video.error) {
                return 3;
            }

            if (element.ended && element.currentTime === video.shownTime) {
                return 2;
            }

            if (element.readyState < HTMLMediaElement.HAVE_CURRENT_DATA || element.currentTime === video.shownTime || width <= 0 || height <= 0) {
                return 0;
            }

            if (!video.canvas || video.canvas.width !== width || video.canvas.height !== height) {
                video.canvas = new OffscreenCanvas(width, height);
                video.context = video.canvas.getContext("2d", { willReadFrequently: true });
            }

            video.context.drawImage(element, 0, 0, width, height);
            pixels.set(video.context.getImageData(0, 0, width, height).data);
            video.shownTime = element.currentTime;
            return 1;
        };

        // Copies the oldest samples that the processor handed over into `samples`, as many as fit, and returns how many it copied.
        const takeSound = (id, samples) => {
            const video = videos.get(id);
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

        const seek = (id, seconds) => {
            const video = videos.get(id);
            video.element.currentTime = seconds;
            video.shownTime = -1;
            video.sentTime = -1;
            video.samples = [];
        };

        const close = (id) => {
            const video = videos.get(id);
            videos.delete(id);
            video.element.pause();
            video.element.removeAttribute("src");
            video.element.load();

            if (video.tap) {
                video.tap.tap.port.postMessage({ ended: true });
                video.tap.tap.port.onmessage = null;
                video.tap.source.disconnect();
                video.tap.tap.disconnect();
                video.tap.silence.disconnect();
            }

            URL.revokeObjectURL(video.url);
        };

        // Videos that the browser muted until the user interacted with the page play their sound from the first tap, click or key.
        const unlock = () => {
            for (const video of videos.values()) {
                if (video.waitingForUser) {
                    video.waitingForUser = false;
                    video.element.muted = false;
                }
            }
        };

        const closeAll = () => {
            for (const id of [...videos.keys()]) {
                close(id);
            }
        };

        return { open, info, control, frame, takeSound, seek, close, unlock, closeAll, time: (id) => videos.get(id).element.currentTime };
    })();

    // Browsers offer AudioWorklet only to pages served over https or from localhost, and some keep the `AudioWorkletNode` constructor elsewhere while their contexts have no `audioWorklet`. Such pages run without sound, and the engine logs why.
    audio.supported = function () {
        return typeof AudioContext !== "undefined" && typeof AudioWorkletNode !== "undefined" && "audioWorklet" in AudioContext.prototype;
    };

    // The contexts of the outputs, one for each sample rate, which the page keeps from the first output on. So an app that runs again plays on the context that the person already unlocked, its processors load once, and no context closes while the next one opens, which some browsers refuse. A context whose outputs all stopped waits suspended, so a stopped app holds no audio hardware, and `audio.dispose` closes every context for good.
    const sharedContext = (sampleRate) => {
        let shared = audio.contexts.get(sampleRate);
        if (shared !== undefined) {
            return shared;
        }

        const context = new AudioContext({ sampleRate, latencyHint: "interactive" });
        const worklet = runtimeFile("haylen-audio-worklet.js");
        shared = { context, worklet, ran: false, ready: context.audioWorklet.addModule(worklet), change: null };
        context.onstatechange = () => {
            shared.ran = shared.ran || context.state === "running";
            reportAudioLocks();
        };
        audio.contexts.set(sampleRate, shared);
        return shared;
    };

    const reportAudioFailure = (action) => (error) => console.error("The audio output could not " + action + ": " + error.message);

    // Browsers run a context only once the user has interacted with the page, unless their autoplay policy lets it run on its own, and Safari on iOS suspends it again after interruptions. A resume without either only makes the browser warn, so the context then waits for the next tap, click or key. A context runs while any of its outputs started, and suspends once none did. One change of a context runs at a time, since WebKit refuses a suspend while a resume is under way, and the context settles again once the change ended, as the outputs may have changed meanwhile.
    const settleContext = (shared) => {
        const context = shared.context;
        if (shared.change !== null || context.state === "closed") {
            return;
        }

        const playing = [...audio.outputs.values()].some((output) => output.shared === shared && output.started);
        const activation = navigator.userActivation;
        const allowed = !activation || activation.hasBeenActive || shared.ran;
        let change = null;
        if (!playing && context.state === "running") {
            change = context.suspend().catch(reportAudioFailure("suspend"));
        } else if (playing && context.state !== "running" && allowed) {
            change = context.resume().catch(reportAudioFailure("resume"));
        }

        if (change === null) {
            return;
        }

        shared.change = change.then(() => {
            shared.change = null;
            settleContext(shared);
        });
    };

    // The processor takes its end of the channel as its first message, and the mixer the other end, with which it starts and stops the processor and posts it the blocks it mixes.
    const connectAudio = (output) => {
        // The ring buffer of the processor holds the whole target, which the mixer never exceeds.
        const options = { channels: output.channels, capacity: output.bufferedFrames, reportFrames: output.blockFrames };
        const context = output.shared.context;
        const channel = new MessageChannel();
        output.node = new AudioWorkletNode(context, "haylen-audio", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [output.channels], processorOptions: options });
        output.node.port.postMessage(channel.port1, [channel.port1]);
        output.node.connect(context.destination);
        engine.connectAudio(output.device, channel.port2);
    };

    // The engine hears of an output that the browser refuses, whose context it does not create or whose processor does not load, and leaves the app without sound with the reason in its log, as it does for an output that the system refuses.
    const failAudio = (device, message) => {
        engine.call("haylen_web_audio_failed", null, ["number", "string"], [device, message]);
    };

    // Takes the context of the sample rate and connects the processor once it loaded, which may still be loading when the engine starts the output. An output that closed while the processor loaded is gone, whatever the loading gave.
    audio.open = function (device, channels, sampleRate, blockFrames, bufferedFrames) {
        let shared;
        try {
            shared = sharedContext(sampleRate);
        } catch (error) {
            failAudio(device, "The browser refused the audio context of the output: " + error.message);
            return;
        }

        const output = { device, channels, blockFrames, bufferedFrames, shared, node: null, started: false };
        audio.outputs.set(device, output);
        const isOpen = () => audio.outputs.get(device) === output;
        shared.ready.then(
            () => {
                if (!isOpen()) {
                    return;
                }

                try {
                    connectAudio(output);
                } catch (error) {
                    failAudio(device, "The browser refused the audio processor of the output: " + error.message);
                }
            },
            (error) => {
                if (isOpen()) {
                    failAudio(device, "The audio processor \"" + shared.worklet + "\" could not be loaded: " + error.message);
                }
            }
        );
    };

    // An output that the browser refused has no context to start or stop.
    audio.start = function (device) {
        const output = audio.outputs.get(device);
        if (output === undefined) {
            return;
        }

        output.started = true;
        settleContext(output.shared);
        reportAudioLocks();
    };

    audio.stop = function (device) {
        const output = audio.outputs.get(device);
        if (output === undefined) {
            return;
        }

        output.started = false;
        settleContext(output.shared);
        reportAudioLocks();
    };

    // The browser releases the node of a closed output, whose processor the mixer ended, while the context stays for the next output.
    audio.close = function (device) {
        const output = audio.outputs.get(device);
        if (output === undefined) {
            return;
        }

        audio.outputs.delete(device);
        if (output.node) {
            output.node.disconnect();
        }

        settleContext(output.shared);
        reportAudioLocks();
    };

    // Closes every context once no output uses it, such as when the page that hosts the runtime goes away, and resolves once the browser closed them.
    audio.dispose = function () {
        if (audio.outputs.size > 0) {
            throw new Error("The audio contexts close only once the app stopped.");
        }

        const closing = [...audio.contexts.values()].map((shared) => Promise.resolve(shared.change).then(() => shared.context.close()));
        audio.contexts.clear();
        return Promise.all(closing);
    };

    // Whether the output that the engine started waits for the user, as browsers keep a context suspended until the first tap, click or key.
    audio.locked = function (device) {
        const output = audio.outputs.get(device);
        return output !== undefined && output.started && output.shared.context.state !== "running";
    };

    // In the Worker mode the worker reads which outputs wait for the user from the state of the page.
    const reportAudioLocks = () => {
        if (hostedInWorker) {
            worker.setState({ audioLocks: [...audio.outputs.keys()].filter((device) => audio.locked(device)) });
        }
    };

    // Describes the output of the app for `onStats`, which is unavailable while the app runs without sound, with what the mixer of the engine side reports.
    audio.stats = function () {
        const [output] = audio.outputs.values();
        if (!output) {
            return { available: false };
        }

        const context = output.shared.context;
        return { available: true, state: context.state, sampleRate: context.sampleRate, ...engine.mixerStats(output.device, context.sampleRate) };
    };

    // Every tap, click and key resumes the outputs the app keeps playing, unless the app is in the background.
    const unlockAudio = () => {
        if (document.visibilityState !== "visible" || engine.paused()) {
            return;
        }

        haylen.video.unlock();
        for (const shared of audio.contexts.values()) {
            settleContext(shared);
        }
    };

    const reportNetwork = () => {
        engine.call("haylen_web_network", null, ["number"], [navigator.onLine ? 1 : 0]);
    };

    // The screen counts as portrait or landscape by the Screen Orientation API, or by the shape of the page where the browser lacks it.
    haylen.orientation = function () {
        const type = screen.orientation ? screen.orientation.type : matchMedia("(orientation: portrait)").matches ? "portrait" : "landscape";
        return type.startsWith("portrait") ? 1 : 0;
    };

    // Fullscreen covers the element that holds the canvas and the overlay of the runtime, so the fields and the views of plugins stay over the app. Browsers grant it only shortly after an input of the person, and Safari before 16.4 names it with its prefix.
    // The Fullscreen API takes the screen to cover once the person let the page see every screen, and a page that is fullscreen on another screen moves there.
    const changeFullscreen = async (value, display) => {
        const holder = Module.canvas.parentElement;
        const current = document.fullscreenElement || document.webkitFullscreenElement;
        const target = pageScreens.details !== null ? pageScreens.details.screens.find((shown) => screenId(shown) === display) : undefined;
        if (value && (current !== holder || target !== undefined)) {
            if (!holder.requestFullscreen && !holder.webkitRequestFullscreen) {
                return { state: "unsupported", message: "The browser has no Fullscreen API." };
            }

            try {
                await (holder.requestFullscreen ? holder.requestFullscreen(target !== undefined ? { screen: target } : {}) : holder.webkitRequestFullscreen());
            } catch (error) {
                return { state: "denied", message: "The browser refused fullscreen: " + error.message + " Browsers go fullscreen only shortly after an input of the person, such as a click or a key." };
            }
        } else if (!value && current === holder) {
            await (document.exitFullscreen ? document.exitFullscreen() : document.webkitExitFullscreen());
        }

        return { state: "granted" };
    };

    // The app is fullscreen while the element that holds the canvas and the overlay covers the screen.
    haylen.isFullscreen = function () {
        return (document.fullscreenElement || document.webkitFullscreenElement) === Module.canvas.parentElement;
    };

    // A newer change replaces the change that waits for the browser, whose call the page denies, and starts once the browser settled that change, so it starts from the mode that change left.
    let fullscreenChange = null;
    haylen.setFullscreen = function (id, value, display) {
        const previous = fullscreenChange;
        if (previous !== null) {
            answerGatedCall(previous.id, { state: "denied", message: "A newer change of fullscreen replaced this one before the window finished it." });
        }

        const change = { id, settled: null };
        change.settled = (async () => {
            if (previous !== null) {
                await previous.settled.catch(() => undefined);
            }

            return changeFullscreen(value, display);
        })();
        fullscreenChange = change;
        settleGatedCall(id, async () => {
            try {
                return await change.settled;
            } finally {
                if (fullscreenChange === change) {
                    fullscreenChange = null;
                }
            }
        });
    };

    // The browser locks the pointer that SDL asks for during an input of the person, at once or at the next one, and tells the page through its events. A lock that no event settles within the bound is one that the browser keeps waiting for an input of the person.
    const kPointerLockMilliseconds = 5000;
    haylen.awaitPointerLock = function (id, value) {
        settleGatedCall(id, () => new Promise((resolve) => {
            if (!value || document.pointerLockElement === Module.canvas) {
                resolve({ state: "granted" });
                return;
            }

            if (typeof Module.canvas.requestPointerLock !== "function") {
                resolve({ state: "unsupported", message: "The browser has no Pointer Lock API." });
                return;
            }

            const refusal = "The browser locks the pointer only during a click or a key of the person, so the app locks it from one.";
            const settle = (answer) => {
                clearTimeout(timer);
                document.removeEventListener("pointerlockchange", changed);
                document.removeEventListener("pointerlockerror", failed);
                resolve(answer);
            };
            const changed = () => settle(document.pointerLockElement === Module.canvas ? { state: "granted" } : { state: "denied", message: refusal });
            const failed = () => settle({ state: "denied", message: refusal });
            const timer = setTimeout(failed, kPointerLockMilliseconds);
            document.addEventListener("pointerlockchange", changed);
            document.addEventListener("pointerlockerror", failed);
        }));
    };

    // The browser releases a wake lock whenever the page hides, so the page asks for it again each time it shows while the app keeps the screen on, and answers the app with the first request.
    let screenKeptOn = false;
    let wakeLock = null;
    let requestingWakeLock = null;
    const requestWakeLock = function () {
        if (!screenKeptOn || wakeLock) {
            return Promise.resolve({ state: "granted" });
        }

        if (document.visibilityState !== "visible") {
            return Promise.resolve({ state: "denied", message: "The browser keeps the screen on only for a page that shows, so the page asks again once it shows." });
        }

        if (requestingWakeLock) {
            return requestingWakeLock;
        }

        requestingWakeLock = navigator.wakeLock.request("screen").then((lock) => {
            wakeLock = lock;
            lock.addEventListener("release", () => {
                wakeLock = null;
            });

            if (!screenKeptOn) {
                lock.release();
            }

            return { state: "granted" };
        }, (error) => ({ state: "denied", message: "The browser refused to keep the screen on: " + error.message })).finally(() => {
            requestingWakeLock = null;
        });
        return requestingWakeLock;
    };

    document.addEventListener("visibilitychange", () => {
        if (navigator.wakeLock) {
            requestWakeLock();
        }
    });

    haylen.keepScreenOn = function (id, value) {
        settleGatedCall(id, () => {
            screenKeptOn = value;

            if (!value) {
                if (wakeLock) {
                    wakeLock.release();
                }

                return { state: "granted" };
            }

            if (!navigator.wakeLock) {
                return { state: "unsupported", message: "The browser has no Screen Wake Lock API, so the screen follows the settings of the system." };
            }

            return requestWakeLock();
        });
    };

    // Browsers let a page write the clipboard only in a secure context, served over https or from localhost, and shortly after an input of the person. Chromium reads it for a page that the person allowed, and the other browsers show a paste control at each read. So a write and a read answer the engine once the browser settles them, and a paste of the person into a field of the UI takes the text of the last paste of the page or the last text it wrote.
    let clipboardText = "";
    window.addEventListener("paste", (event) => {
        clipboardText = event.clipboardData.getData("text/plain");
        reportClipboard();
    }, true);

    // The worker of the Worker mode reads the text from the state of the page.
    const reportClipboard = () => {
        if (hostedInWorker) {
            worker.setState({ clipboard: clipboardText });
        }
    };

    haylen.clipboardText = function () {
        return clipboardText;
    };

    const withoutClipboard = (action) => ({ state: "unsupported", message: "The page cannot " + action + " the clipboard, because browsers give the clipboard only to pages served over https or from localhost." });
    haylen.writeClipboard = function (id, text) {
        settleGatedCall(id, async () => {
            if (!navigator.clipboard || typeof navigator.clipboard.writeText !== "function") {
                return withoutClipboard("write");
            }

            try {
                await navigator.clipboard.writeText(text);
            } catch (error) {
                return { state: "denied", message: "The browser refused to write the clipboard: " + error.message + " Browsers write it only shortly after an input of the person, such as a click or a key." };
            }

            clipboardText = text;
            reportClipboard();
            return { state: "granted" };
        });
    };

    haylen.readClipboard = function (id) {
        settleGatedCall(id, async () => {
            if (!navigator.clipboard || typeof navigator.clipboard.readText !== "function") {
                return withoutClipboard("read");
            }

            try {
                const text = await navigator.clipboard.readText();
                return { state: "granted", text };
            } catch (error) {
                const refused = (await queryPermission("clipboard-read")) === "denied";
                return { state: "denied", message: refused ? "The person has not allowed the page to read the clipboard, which the settings of the site in the browser allow again." : "The browser did not let the page read the clipboard: " + error.message };
            }
        });
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

    // The scrolls of wheels and trackpads reach the engine with their units: precise devices, which browsers report in pixels, move by the points of the page, and wheels by their steps, of three lines each. The modifiers go as the bits of shift, control, alt and the super key, from the lowest.
    const readModifiers = (event) => (event.shiftKey ? 1 : 0) | (event.ctrlKey ? 2 : 0) | (event.altKey ? 4 : 0) | (event.metaKey ? 8 : 0);
    const readScroll = (event) => {
        const precise = event.deltaMode === WheelEvent.DOM_DELTA_PIXEL;
        const scale = precise ? -1 : event.deltaMode === WheelEvent.DOM_DELTA_LINE ? -1 / 3 : -10;
        return [event.deltaX * scale, event.deltaY * scale, precise ? 1 : 0, readModifiers(event)];
    };

    // Once the runtime runs, the page takes the work that waited for it and follows the canvas and the document. On the page, SDL follows window resizes only, so a canvas resized by the page layout announces its new size as a window resize, and SDL leaves out the units of scrolls. In the Worker mode the page posts all of its input to the worker. A hidden page sends the app to the background, a page that goes away makes the user data durable, and the network state, the time zone, the color scheme, the languages, the accessibility preferences and the battery reach the app when it starts and whenever they change.
    const startPage = () => {
        if (hostedInWorker) {
            worker.watchInput();
        } else {
            new ResizeObserver(() => window.dispatchEvent(new Event("resize"))).observe(Module.canvas);
            Module.canvas.addEventListener("wheel", (event) => {
                engine.call("haylen_web_scroll", null, ["number", "number", "number", "number"], readScroll(event));
                event.preventDefault();
            }, { passive: false });
        }

        waiting.runtimeReady = true;
        for (const work of waiting.runtime.splice(0)) {
            work();
        }

        Module.canvas.addEventListener("pointerdown", notePress, true);
        Module.canvas.addEventListener("mousedown", keepEditingFocus, true);
        Module.canvas.addEventListener("pointerup", focusTapped, true);
        Module.canvas.addEventListener("pointercancel", (event) => presses.delete(event.pointerId), true);
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
            if (document.visibilityState === "visible") {
                reportTimeZone();
            }

            reportVisibility();
            unlockAudio();
        });
        window.addEventListener("focus", reportTimeZone);
        // A browser that freezes a hidden page, as Chromium does with the Page Lifecycle API, may discard it without another event, so the files become durable before it freezes too, and a page that resumes takes the state that its visibility tells.
        window.addEventListener("pagehide", () => engine.call("haylen_web_page_hidden", null, [], []));
        document.addEventListener("freeze", () => engine.call("haylen_web_page_hidden", null, [], []));
        document.addEventListener("resume", reportVisibility);
        window.addEventListener("online", reportNetwork);
        window.addEventListener("offline", reportNetwork);
        reportNetwork();
        reportTimeZone();
        reportTheme();
        device.darkScheme.addEventListener("change", reportTheme);
        window.addEventListener("languagechange", reportLanguages);
        reportAccessibility();

        for (const query of [device.reducedMotion, device.highContrast, device.reducedTransparency]) {
            query.addEventListener("change", reportAccessibility);
        }

        if (device.battery) {
            reportBattery();

            for (const type of ["levelchange", "chargingchange", "chargingtimechange"]) {
                device.battery.addEventListener(type, reportBattery);
            }
        }

        for (const type of ["pointerdown", "touchend", "keydown"]) {
            window.addEventListener(type, unlockAudio, { capture: true, passive: true });
        }
    };

    // The worker of the Worker mode stops its animation frames while the page hides, so it learns of the visibility of the page too.
    const reportVisibility = () => {
        const visible = document.visibilityState === "visible" ? 1 : 0;
        engine.call("haylen_web_visibility", null, ["number"], [visible]);

        if (hostedInWorker) {
            engine.call("haylen_worker_visibility", null, ["number"], [visible]);
        }
    };

    // The page reads the device before the runtime starts, so the engine knows it on the first frame.
    if (!hostedInWorker) {
        Module.preRun = Module.preRun || [];
        Module.preRun.push(function () {
            addRunDependency("haylen-device");
            readDevice().then(() => removeRunDependency("haylen-device"));
        });
        Module.postRun = Module.postRun || [];
        Module.postRun.push(startPage);
        return;
    }

    // The Worker mode: the runtime runs in a dedicated worker that `haylen-worker.js` starts next to the runtime script of `Module.haylen.runtimeUrl`, `haylen.js` next to the page by default. The worker draws into the canvas as an `OffscreenCanvas` and reaches the page through messages: the page posts the input over the canvas, the state of the page that the engine reads during a frame, the requests of the editor and the calls of the page into the engine, and the worker posts the calls of the engine into the page, the answers of the editor and a heartbeat. So an editor ends a runtime that stops answering with `Module.haylen.editor.terminate()` while the page goes on.
    const worker = { thread: null, queued: [], requests: new Map(), nextRequest: 1, heartbeat: 0, ended: null, mixer: {}, paused: false, pointerWanted: false, gamepads: { revision: 0, key: "", sent: "" } };

    // The kinds of input that the worker translates, in the order of `worker::Runtime::Change` and `Touches::Phase` of the engine.
    const changes = { focusGained: 0, focusLost: 1, mouseEnter: 2, mouseLeave: 3, resized: 4 };
    const phases = { pointerdown: 0, pointermove: 1, pointerup: 2, pointercancel: 3 };
    const mouseChanges = { pointerdown: 0, pointerup: 1, pointermove: 2 };

    // What the page posts before the worker started waits for it, and an ended worker hears nothing more.
    worker.post = (message, transfer) => {
        if (worker.thread !== null) {
            worker.thread.postMessage(message, transfer || []);
        } else if (worker.ended === null) {
            worker.queued.push({ message, transfer });
        }
    };

    worker.setState = (values) => worker.post({ type: "state", values });

    // The calls of the page into the engine, which the worker makes in the order they arrive, and what the page side asks of the engine side.
    engine = {
        call: (name, returns, types, values) => worker.post({ type: "engine", name, types, values }),
        paused: () => worker.paused,
        connectAudio: (device, port) => worker.post({ type: "audioPort", device, port }, [port]),
        mixerStats: () => worker.mixer,
        openStream: (key, kind, args) => worker.post({ type: "stream", operation: "open", key, kind, args }),
        pushVideo: (key, pixels, width, height, time) => {
            worker.post({ type: "stream", operation: "video", key, pixels, width, height, time }, [pixels.buffer]);
            return true;
        },
        pushAudio: (stream, samples, channels) => {
            const frames = Math.min(samples.length / channels, stream.room);
            if (frames > 0) {
                stream.room -= frames;
                worker.post({ type: "stream", operation: "audio", key: stream.key, channels, samples: samples.slice(0, frames * channels) });
            }

            return frames;
        },
    };

    // The state of the page that the engine reads during a frame: the size of the canvas box in CSS pixels with the pixels of the screen in each, the safe area, the fold, the orientation, fullscreen, the text of the clipboard and the screens, which the page posts again whenever they change.
    const readCanvasBox = () => [Module.canvas.clientWidth, Module.canvas.clientHeight, window.devicePixelRatio || 1];
    const pageState = () => ({ canvasBox: readCanvasBox(), safeArea: haylen.safeAreaInsets(), fold: haylen.fold(), orientation: haylen.orientation(), fullscreen: haylen.isFullscreen(), clipboard: clipboardText, displays: haylen.displays(), nativePlugins: haylen.nativePlugins() });
    worker.postLayout = () => {
        forgetLayout();
        worker.setState(pageState());
    };

    // The keys that reach the app, which the worker reads by their `code`. The page keeps the keys that would scroll it or move its focus from doing so while they go to the app.
    const pageKeys = new Set(["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space", "Tab", "Backspace", "PageUp", "PageDown", "Home", "End"]);
    worker.forwardKey = (event) => {
        if (event.type !== "keydown" && event.type !== "keyup") {
            return;
        }

        if (pageKeys.has(event.code)) {
            event.preventDefault();
        }

        engine.call("haylen_worker_key", null, ["string", "number", "number", "number"], [event.code, event.type === "keydown" ? 1 : 0, event.repeat ? 1 : 0, readModifiers(event)]);
    };

    // Reads the controllers in the standard mapping of the Gamepad API at every animation frame of the page and posts them whenever they changed, with a new revision whenever one connected or left.
    const readGamepads = () => {
        const pads = [];
        for (const pad of navigator.getGamepads ? navigator.getGamepads() : []) {
            if (pad && pad.connected && pad.mapping === "standard") {
                const buttons = Array.from({ length: 17 }, (_, place) => {
                    const button = pad.buttons[place];
                    return button === undefined ? 0 : place === 6 || place === 7 ? button.value : button.pressed ? 1 : 0;
                });
                const axes = Array.from({ length: 4 }, (_, place) => pad.axes[place] || 0);
                pads.push({ index: pad.index, id: pad.id, buttons, axes });
            }
        }

        const key = pads.map((pad) => pad.index + ":" + pad.id).join("\n");
        if (key !== worker.gamepads.key) {
            worker.gamepads.key = key;
            worker.gamepads.revision += 1;
        }

        const sent = JSON.stringify(pads);
        if (sent !== worker.gamepads.sent) {
            worker.gamepads.sent = sent;
            worker.setState({ gamepads: { revision: worker.gamepads.revision, pads } });
        }

        if (worker.ended === null) {
            requestAnimationFrame(readGamepads);
        }
    };

    // Pointers of a mouse go to the engine as the mouse, captured by the canvas while a button is down, and touches and pens as fingers, with their positions in CSS pixels of the canvas box. A press locks the pointer that the app asked to lock and the browser has not locked yet.
    worker.watchInput = () => {
        const canvas = Module.canvas;
        const pointer = (event) => {
            const finger = event.pointerType === "touch" || event.pointerType === "pen";
            if (event.type === "pointerdown" && worker.pointerWanted && document.pointerLockElement !== canvas && !finger) {
                canvas.requestPointerLock();
            }

            if (finger) {
                engine.call("haylen_worker_touch", null, ["number", "number", "number", "number", "number"], [event.pointerId, phases[event.type], event.offsetX, event.offsetY, readModifiers(event)]);
                return;
            }

            if (event.type === "pointercancel") {
                return;
            }

            if (event.type === "pointerdown") {
                canvas.setPointerCapture(event.pointerId);
            }

            engine.call("haylen_worker_mouse", null, ["number", "number", "number", "number", "number", "number", "number"], [mouseChanges[event.type], event.button, event.offsetX, event.offsetY, event.movementX || 0, event.movementY || 0, readModifiers(event)]);
        };

        for (const type of ["pointerdown", "pointermove", "pointerup", "pointercancel"]) {
            canvas.addEventListener(type, pointer);
        }

        canvas.addEventListener("pointerenter", () => engine.call("haylen_worker_change", null, ["number"], [changes.mouseEnter]));
        canvas.addEventListener("pointerleave", () => engine.call("haylen_worker_change", null, ["number"], [changes.mouseLeave]));
        canvas.addEventListener("wheel", (event) => {
            engine.call("haylen_worker_scroll", null, ["number", "number", "number", "number"], readScroll(event));
            event.preventDefault();
        }, { passive: false });
        window.addEventListener("focus", () => engine.call("haylen_worker_change", null, ["number"], [changes.focusGained]));
        window.addEventListener("blur", () => engine.call("haylen_worker_change", null, ["number"], [changes.focusLost]));

        const resized = () => {
            worker.postLayout();
            engine.call("haylen_worker_change", null, ["number"], [changes.resized]);
        };
        new ResizeObserver(resized).observe(canvas);
        window.addEventListener("resize", worker.postLayout);
        window.addEventListener("scroll", worker.postLayout, { passive: true });
        document.addEventListener("fullscreenchange", worker.postLayout);
        document.addEventListener("webkitfullscreenchange", worker.postLayout);

        if (screen.orientation) {
            screen.orientation.addEventListener("change", worker.postLayout);
        }

        if (navigator.devicePosture) {
            navigator.devicePosture.addEventListener("change", worker.postLayout);
        }

        // A change of the zoom or of the screen changes the pixels of the screen in each CSS pixel, which the canvas box follows.
        const watchDensity = () => {
            const query = matchMedia("(resolution: " + (window.devicePixelRatio || 1) + "dppx)");
            query.addEventListener("change", () => {
                resized();
                watchDensity();
            }, { once: true });
        };
        watchDensity();
        reportVisibility();
        readGamepads();
    };

    // The worker asks the page for what only the document has.
    haylen.setTitle = function (title) {
        document.title = title;
    };

    haylen.setCursor = function (cursor) {
        Module.canvas.style.cursor = cursor;
    };

    // The Gamepad API runs the motors through the vibration actuator of the controller, the strong motor at the low intensity and the weak one at the high intensity, where the browser offers it, and ignores the request elsewhere, as the controllers of the engine do without motors.
    haylen.rumbleGamepad = function (index, low, high, seconds) {
        const pad = navigator.getGamepads ? navigator.getGamepads()[index] : null;
        const actuator = pad ? pad.vibrationActuator : null;
        if (!actuator || typeof actuator.playEffect !== "function") {
            return;
        }

        if (seconds <= 0 || (low <= 0 && high <= 0)) {
            actuator.reset().catch(() => {});
            return;
        }

        actuator.playEffect("dual-rumble", { duration: seconds * 1000, strongMagnitude: low, weakMagnitude: high }).catch(() => {});
    };

    // The page asks the browser for the lock now, which it grants during an input of the person, and again at the next press on the canvas.
    haylen.lockPointer = function (id, value) {
        worker.pointerWanted = value;
        if (value && document.pointerLockElement !== Module.canvas) {
            Module.canvas.requestPointerLock();
        } else if (!value && document.pointerLockElement === Module.canvas) {
            document.exitPointerLock();
        }

        haylen.awaitPointerLock(id, value);
    };

    // The editor contract of the Worker mode: the requests go to the worker, which answers them with the contract of the runtime on the page, and `hello` reports `workerHosting`. The page adds what only it can do: `heartbeat()` answers the milliseconds since the worker last answered, which grow while code of the app holds the worker, and `terminate()` ends the worker at once, rejects every request that waits with the code `terminated` and releases what the page holds for the runtime.
    const editor = { protocol: 1 };
    haylen.editor = editor;

    const editorError = (code, message) => Object.assign(new Error(message), { code });

    editor.request = function (method, params = {}) {
        return new Promise((resolve, reject) => {
            if (worker.ended !== null) {
                reject(editorError(worker.ended, worker.ended === "disposed" ? "The runtime was disposed of, so it takes no more requests." : "The worker of the runtime was terminated, so it takes no more requests."));
                return;
            }

            const id = worker.nextRequest++;
            worker.requests.set(id, { method, resolve, reject });
            worker.post({ type: "request", id, method, params });
        });
    };

    editor.heartbeat = function () {
        return performance.now() - worker.heartbeat;
    };

    // Releases what the page holds for the runtime, which no worker uses any more.
    const releasePage = (ending, message) => {
        worker.ended = ending;
        worker.queued.length = 0;

        if (worker.thread !== null) {
            worker.thread.terminate();
            worker.thread = null;
        }

        for (const request of worker.requests.values()) {
            request.reject(editorError(ending, message));
        }

        worker.requests.clear();
        text.finish();
        haylen.video.closeAll();
        audio.outputs.clear();

        for (const number of [...pageOutputs.keys()]) {
            haylen.closeOutput(number);
        }

        if (wakeLock) {
            wakeLock.release();
        }

        if (document.pointerLockElement === Module.canvas) {
            document.exitPointerLock();
        }

        return audio.dispose();
    };

    editor.terminate = function () {
        if (worker.ended !== null) {
            return Promise.resolve();
        }

        return releasePage("terminated", "The worker of the runtime was terminated before it answered.").then(() => notify("onStopped"));
    };

    // The page ends the worker once it disposed of the runtime, after the page closed its audio contexts.
    const answer = (message) => {
        const request = worker.requests.get(message.id);
        worker.requests.delete(message.id);
        if (request === undefined) {
            return;
        }

        if (!message.ok) {
            request.reject(editorError(message.error.code, message.error.message));
            return;
        }

        if (request.method === "pause" || request.method === "resume") {
            worker.paused = message.result.paused;
        }

        if (request.method !== "dispose") {
            request.resolve(message.result);
            return;
        }

        releasePage("disposed", "The runtime was disposed of, so it takes no more requests.").then(() => request.resolve(message.result), request.reject);
    };

    // Runs a call of the engine into the page by the path of its function on `Module.haylen`. The statistics carry the numbers of the mixer, which the page merges with what its audio context tells.
    const callPage = (path, values) => {
        if (path === "reportStats") {
            worker.mixer = values[0].mixer;
            delete values[0].mixer;
        }

        const names = path.split(".");
        const owner = names.slice(0, -1).reduce((value, name) => value[name], haylen);
        owner[names[names.length - 1]](...values);
    };

    const receive = (event) => {
        const message = event.data;
        switch (message.type) {
        case "page":
            callPage(message.path, message.values);
            return;
        case "ready":
            startPage();
            worker.postLayout();
            return;
        case "answer":
            answer(message);
            return;
        case "heartbeat":
            worker.heartbeat = performance.now();
            return;
        default:
            throw new Error("The worker of the runtime sent the unknown message \"" + message.type + "\".");
        }
    };

    // A failure that escapes the code of the worker, such as one of its script while it loads, reaches the page as an error of the runtime.
    const fail = (event) => {
        event.preventDefault();
        haylen.reportError({ message: "The worker of the runtime failed: " + event.message, file: "", line: 0, traceback: "" });
    };

    // A page holds the start of the worker as it holds the module on the page: the functions of `Module.preRun` run first, and `Module.addRunDependency` holds the start until `Module.removeRunDependency` releases every dependency, as a loader does while it loads the web modules of plugins.
    const runPreRun = () => {
        const dependencies = new Set();
        let release = null;
        const released = new Promise((resolve) => (release = resolve));
        Module.addRunDependency = (name) => dependencies.add(name);
        Module.removeRunDependency = (name) => {
            dependencies.delete(name);
            if (dependencies.size === 0) {
                release();
            }
        };

        for (const work of [].concat(Module.preRun || [])) {
            work();
        }

        if (dependencies.size === 0) {
            release();
        }

        return released;
    };

    // Starts the worker once the page read the device, with the canvas, the options of the page and the state that the engine reads as it starts. The bytes of a package that the page holds go to the worker, and the addresses of the page become absolute, since the worker resolves addresses against its own script.
    const startWorker = async () => {
        await Promise.all([readDevice(), runPreRun()]);

        // An editor that ended the runtime while the page read the device starts no worker.
        if (worker.ended !== null) {
            return;
        }

        const script = new URL(haylen.runtimeUrl || "haylen.js", document.baseURI).href;
        const offscreen = Module.canvas.transferControlToOffscreen();
        const transfer = [offscreen];
        const options = {
            development: haylen.development === true,
            developmentServer: haylen.developmentServer,
            graphicsApi: haylen.graphicsApi,
            arguments: haylen.arguments,
            storageNamespace: haylen.storageNamespace,
            projectBudget: haylen.projectBudget,
            packageUrl: haylen.packageUrl ? new URL(haylen.packageUrl, document.baseURI).href : undefined,
            packageData: haylen.packageData,
            wasmBinary: Module.wasmBinary,
            release: haylen.release ? { ...haylen.release, address: new URL(haylen.release.address, document.baseURI).href } : undefined,
        };
        const state = { ...pageState(), systemInfo: haylen.systemInfo(), languages: haylen.languages(), audioSupported: audio.supported(), audioLocks: [], gamepads: { revision: 0, pads: [] } };
        haylen.packageData = null;
        Module.wasmBinary = null;
        worker.heartbeat = performance.now();
        worker.thread = new Worker(new URL("haylen-worker.js", script));
        worker.thread.onmessage = receive;
        worker.thread.onerror = fail;
        worker.post({ type: "start", script, canvas: offscreen, options, state }, transfer);

        for (const { message, transfer: moved } of worker.queued.splice(0)) {
            worker.post(message, moved);
        }
    };

    startWorker().catch((error) => haylen.reportError({ message: "The worker of the runtime could not start: " + error.message, file: "", line: 0, traceback: "" }));

})(Module.haylen);
