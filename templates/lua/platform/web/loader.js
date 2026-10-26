// Loads a Haylen app into the page: checks what the browser supports, downloads the runtime with the manifests and the app domain of the protected release, or with `app.zip` in development, behind a real progress bar, imports the web modules of the plugins, and hands everything to the runtime.
// The script `haylen.py` writes `config.json` next to this file with the name, the identifier, the transparency, the splash logo and backgrounds of `app.json`, the files of the release that download before the app starts, the size of every download and the plugins with a web part, whose modules it copies to `plugins/<id>/`.

var Module = {
    canvas: document.getElementById("canvas"),
    preRun: [],
    haylen: {},
};

(async function () {
    const splash = document.getElementById("splash");
    const bar = document.getElementById("splash-bar");
    const progress = document.getElementById("splash-progress");
    const status = document.getElementById("splash-status");
    const details = document.getElementById("splash-details");
    const logo = document.getElementById("splash-logo");

    const fail = (message, detail) => {
        splash.classList.remove("hidden");
        splash.classList.add("failed");
        status.textContent = message;
        if (detail) {
            details.textContent = detail;
            details.hidden = false;
        }
    };

    // The runtime draws through WebGPU or WebGL2, whichever starts first, or tries first the one that `?graphicsApi=` names by the name the engine gives it, `webGpu` or `webGl2`. A browser with neither cannot run the app.
    const checkGraphics = async () => {
        const adapter = navigator.gpu ? await navigator.gpu.requestAdapter().catch(() => null) : null;
        if (adapter === null && document.createElement("canvas").getContext("webgl2") === null) {
            throw new Error("This browser supports neither WebGPU nor WebGL2. Update it or turn on hardware acceleration to run this app.");
        }
        const requested = new URLSearchParams(location.search).get("graphicsApi");
        if (requested) {
            Module.haylen.graphicsApi = requested;
        }
    };

    // Downloads every file into memory while one bar shows the progress of all of them. Compressed responses report their compressed length, so the sizes of `config.json` give the total whenever the length of the actual bytes is unknown.
    const download = async (files) => {
        const received = new Array(files.length).fill(0);
        const totals = files.map((file) => file.size || 0);
        const update = () => {
            const total = totals.reduce((sum, size) => sum + size, 0);
            const loaded = received.reduce((sum, size) => sum + size, 0);
            const percent = total > 0 ? Math.min(100, Math.round((loaded * 100) / total)) : 0;
            bar.style.width = percent + "%";
            progress.setAttribute("aria-valuenow", String(percent));
        };
        return Promise.all(
            files.map(async (file, index) => {
                const response = await fetch(file.url);
                if (!response.ok) {
                    throw new Error("The file \"" + file.url + "\" could not be downloaded: " + response.status + " " + response.statusText + ".");
                }
                const length = Number(response.headers.get("Content-Length"));
                if (!response.headers.get("Content-Encoding") && length > 0) {
                    totals[index] = length;
                }
                const reader = response.body.getReader();
                const chunks = [];
                for (;;) {
                    const { done, value } = await reader.read();
                    if (done) {
                        break;
                    }
                    chunks.push(value);
                    received[index] += value.length;
                    update();
                }
                const bytes = new Uint8Array(received[index]);
                let offset = 0;
                for (const chunk of chunks) {
                    bytes.set(chunk, offset);
                    offset += chunk.length;
                }
                return bytes;
            })
        );
    };

    const loadScript = (url) =>
        new Promise((resolve, reject) => {
            const script = document.createElement("script");
            script.src = url;
            script.onload = resolve;
            script.onerror = () => reject(new Error("The script \"" + url + "\" could not be loaded."));
            document.body.appendChild(script);
        });

    // Imports the module of every plugin, each of which exports its load function as its default export.
    const importPlugins = (plugins) =>
        Promise.all(
            plugins.map(async (plugin) => {
                let module;
                try {
                    module = await import(new URL(plugin.module, document.baseURI).href);
                } catch (error) {
                    throw new Error("The plugin \"" + plugin.id + "\" could not be imported from \"" + plugin.module + "\": " + (error.message || error));
                }
                if (typeof module.default !== "function") {
                    throw new Error("The plugin \"" + plugin.id + "\" does not export \"load(context)\" as the default export of \"" + plugin.module + "\".");
                }
                return { plugin, load: module.default };
            })
        );

    // Loads the plugins in the order of `config.json`, which puts every plugin after the plugins it requires, once the runtime exists and before the app starts. Each load gets the context the runtime makes for the plugin and may return a promise, and a plugin that fails keeps the app from starting.
    const loadPlugins = (plugins) => {
        Module.preRun.push(() => {
            Module.addRunDependency("haylen-plugins");
            (async () => {
                for (const { plugin, load } of plugins) {
                    try {
                        await load(Module.haylen.createPluginContext(plugin.id, plugin.config));
                    } catch (error) {
                        fail("The plugin \"" + plugin.id + "\" could not be loaded.", String(error.message || error));
                        return;
                    }
                }
                Module.removeRunDependency("haylen-plugins");
            })();
        });
    };

    let config;
    try {
        config = await (await fetch("config.json")).json();
    } catch (error) {
        fail("The app configuration could not be loaded.", String(error.message || error));
        return;
    }
    document.title = config.name;
    // A transparent app lets the page behind its canvas show through, so only the splash keeps the background of `app.json`, which has a dark variant while the system uses dark colors.
    const dark = config.splash.darkBackground && matchMedia("(prefers-color-scheme: dark)").matches;
    const background = dark ? config.splash.darkBackground : config.splash.background;
    if (config.transparent) {
        document.documentElement.style.background = "transparent";
        document.body.style.background = "transparent";
        splash.style.background = background;
    } else {
        document.body.style.background = background;
    }
    // The splash and the icon of the page show the logo from a single download.
    logo.alt = config.name;
    fetch(config.splash.logo)
        .then((response) => response.blob())
        .then((blob) => {
            const source = URL.createObjectURL(blob);
            logo.src = source;
            document.querySelector("link[rel=icon]").href = source;
        });

    if (typeof WebAssembly !== "object") {
        fail("This browser does not support WebAssembly, which this app needs.");
        return;
    }

    // The page shows the error screen of the runtime once the app runs, so only failures before that replace the splash, which fades out once the runtime ends it.
    let started = false;
    Module.haylen.onStarted = () => {
        started = true;
    };
    Module.haylen.onSplashEnded = ({fadeOut}) => {
        splash.style.transitionDuration = `${fadeOut}s`;
        splash.classList.add("hidden");
    };
    Module.haylen.onError = (error) => {
        console.error(error.message + (error.traceback ? "\n" + error.traceback : ""));
        if (!started) {
            fail("The app could not start.", error.message + (error.traceback ? "\n\n" + error.traceback : ""));
        }
    };

    try {
        await checkGraphics();
        status.textContent = "Loading " + config.name;
        // A protected release downloads only its manifests and its app domain before the app starts, and the content streams in as the app needs it, while development downloads the zipped package.
        const files = config.release ? config.release.files.map((name) => ({ url: "app/" + name, size: config.sizes["app/" + name] })) : [{ url: "app.zip", size: config.sizes["app.zip"] }];
        const [plugins, [wasm, ...packageFiles]] = await Promise.all([importPlugins(config.plugins), download([{ url: "haylen.wasm", size: config.sizes["haylen.wasm"] }, ...files])]);
        status.textContent = "Starting " + config.name;
        // The runtime compiles the bytes downloaded here, so the browser fetches the WebAssembly module only once.
        Module.instantiateWasm = (imports, receive) => {
            WebAssembly.instantiate(wasm, imports).then(
                (result) => receive(result.instance),
                (error) => fail("The app could not be loaded.", String(error.message || error))
            );
        };
        if (config.release) {
            Module.haylen.release = { address: "app/", identifier: config.identifier, files: config.release.files.map((name, index) => ({ name, bytes: packageFiles[index] })) };
        } else {
            Module.haylen.packageData = packageFiles[0];
        }
        // A run of `haylen.py run --platform web` serves the page in development, connected to the server that pushes every saved file, while a site that `haylen.py prepare` makes has no such entry.
        if (config.development) {
            Module.haylen.development = true;
            Module.haylen.developmentServer = (location.protocol === "https:" ? "wss://" : "ws://") + location.host + config.development.path + "?token=" + encodeURIComponent(config.development.token);
        }
        loadPlugins(plugins);
        await loadScript("haylen.js");
    } catch (error) {
        fail("The app could not be loaded.", String(error.message || error));
    }
})();
