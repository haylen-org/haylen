// Web part of the Native Demo plugin, built on the DOM alone. The page imports this module before the runtime starts and calls its load function with the context of the plugin.
const language = "JavaScript";

// Counts the primes below the limit in slices, yielding to the page between them, since the web runtime has one thread.
const countPrimes = async (limit) => {
    const composite = new Uint8Array(Math.max(limit, 0));
    let count = 0;
    for (let number = 2; number < limit; ++number) {
        if (number % 20000 === 0) {
            await new Promise((resolve) => setTimeout(resolve, 0));
        }
        if (composite[number]) {
            continue;
        }
        count += 1;
        for (let multiple = number * number; multiple < limit; multiple += number) {
            composite[multiple] = 1;
        }
    }
    return count;
};

const failure = (message, code, data) => Object.assign(new Error(message), { code, data });

const bannerColor = (config) => {
    if (!/^#[0-9A-Fa-f]{6}$/.test(config.bannerColor)) {
        throw failure("The bannerColor parameter must be a color as #RRGGBB, not " + config.bannerColor + ".", "invalidColor");
    }
    return config.bannerColor;
};

// The colored bar with the greeting and a Tap button, which the overlay places over the canvas. Clicks outside the bar reach the app.
const createBanner = (context, tapped) => {
    const bar = document.createElement("div");
    bar.style.cssText = "display:flex;align-items:center;gap:12px;padding:0 8px 0 16px;border-radius:10px;color:#fff;font:600 16px system-ui,sans-serif;background:" + bannerColor(context.config);
    const label = document.createElement("span");
    label.textContent = context.config.greeting;
    label.style.cssText = "flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;";
    const button = document.createElement("button");
    button.textContent = "Tap";
    button.style.cssText = "font:600 15px system-ui,sans-serif;padding:8px 16px;border:0;border-radius:8px;background:#fff;color:#1d3557;cursor:pointer;";
    button.addEventListener("click", tapped);
    // The button leaves the focus, and so the keyboard, with the app, like the views over the app on the other platforms.
    button.addEventListener("mousedown", (event) => event.preventDefault());
    bar.append(label, button);
    return bar;
};

// A modal dialog over the whole page with a title, a line of text and a Close button. Escape closes it too, and it resolves once it closed.
const showScreen = (title, color) =>
    new Promise((resolve) => {
        const dialog = document.createElement("dialog");
        dialog.style.cssText = "width:100vw;height:100vh;max-width:none;max-height:none;margin:0;border:0;padding:0;color:#fff;font:18px system-ui,sans-serif;background:" + color;
        const column = document.createElement("div");
        column.style.cssText = "height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:24px;text-align:center;padding:32px;box-sizing:border-box;";
        const heading = document.createElement("h1");
        heading.textContent = title;
        heading.style.cssText = "margin:0;font:700 40px system-ui,sans-serif;";
        const detail = document.createElement("p");
        detail.textContent = "This modal element covers the app, which stands still and stays silent until Close ends the cover.";
        detail.style.margin = "0";
        const close = document.createElement("button");
        close.textContent = "Close";
        close.style.cssText = "font:600 18px system-ui,sans-serif;padding:12px 32px;border:0;border-radius:10px;background:#fff;color:#1d3557;cursor:pointer;";
        close.addEventListener("click", () => dialog.close());
        column.append(heading, detail, close);
        dialog.append(column);
        dialog.addEventListener("close", () => {
            dialog.remove();
            resolve();
        });
        document.body.append(dialog);
        dialog.showModal();
        close.focus();
    });

// Lets the person pick a file with a file input. The browser opens the chooser only while a click, a tap or a key press of the person is recent.
const pickFile = () =>
    new Promise((resolve, reject) => {
        if (navigator.userActivation && !navigator.userActivation.isActive) {
            reject(failure("The browser opens a file chooser only right after a click, a tap or a key press.", "noUserGesture"));
            return;
        }
        const input = document.createElement("input");
        input.type = "file";
        input.addEventListener("change", () => resolve(input.files.length > 0 ? { name: input.files[0].name } : null));
        input.addEventListener("cancel", () => resolve(null));
        input.click();
    });

export default function load(context) {
    let ticker = null;
    let ticks = 0;
    let banner = null;
    let bannerElement = null;
    let bannerState = null;
    let bannerTaps = 0;
    let lastError = null;

    const stopTicking = () => {
        clearInterval(ticker);
        ticker = null;
    };

    context.register("echo", (params) => ({ echo: params.value === undefined ? null : params.value, thread: "main", language }));

    context.register("compute", async (params) => ({ primes: await countPrimes(params.limit), thread: "main", detail: "slices on the main thread that yield to the page between them", language }));

    context.register("fail", () => {
        throw failure("The native demo failed on purpose.", "demoFailure", { reason: "requested", language });
    });

    // The call never answers by itself, so only a cancel or a timeout of the app aborts it, and the plugin tells the app that it heard it.
    context.register(
        "wait",
        (params, { signal }) =>
            new Promise((resolve, reject) => {
                signal.addEventListener("abort", () => {
                    context.emit("waitCancelled", { token: params.token, language });
                    reject(failure("The app gave the call up.", "cancelled"));
                });
            }),
    );

    context.register("config", () => context.config);

    // Every app that loads the Lua API sends start. The plugin ends what an earlier app of the page left running and hands the new app the error that stopped the earlier one.
    context.register("start", () => {
        stopTicking();
        if (banner) {
            banner.remove();
            banner = null;
        }
        if (lastError) {
            context.emit("lastError", lastError, { retain: true });
            lastError = null;
        }
        return null;
    });

    context.register("ticks", (params) => {
        const interval = params.interval;
        stopTicking();
        if (params.enabled) {
            ticks = 0;
            ticker = setInterval(() => {
                ticks += 1;
                context.emit("tick", { count: ticks, thread: "main", language });
            }, interval * 1000);
        }
        return { enabled: params.enabled, interval };
    });

    context.register("showBanner", (params) => {
        if (params.anchor !== "top" && params.anchor !== "bottom") {
            throw failure("The anchor of the banner is top or bottom, not " + params.anchor + ".", "invalidAnchor");
        }
        const placement = { anchor: params.anchor, reserve: params.reserve, width: 360, height: 56 };
        if (banner) {
            banner.update(placement);
        } else {
            bannerElement = createBanner(context, () => {
                bannerTaps += 1;
                context.emit("bannerTapped", { count: bannerTaps, language });
            });
            banner = context.overlay.add(bannerElement, placement);
        }
        bannerState = { anchor: params.anchor, reserve: params.reserve, visible: bannerElement.style.visibility !== "hidden" };
        return bannerState;
    });

    context.register("setBannerVisible", (params) => {
        if (!banner) {
            throw failure("No banner shows. Call showBanner first.", "noBanner");
        }
        banner.setVisible(params.visible);
        bannerState = { ...bannerState, visible: params.visible };
        return bannerState;
    });

    context.register("removeBanner", () => {
        if (banner) {
            banner.remove();
            banner = null;
        }
        return null;
    });

    // The modal screen covers the app while it shows, so the app stands still and stays silent until Close ends the cover.
    context.register("showScreen", async (params) => {
        const color = bannerColor(context.config);
        const started = performance.now();
        context.coverApp();
        try {
            await showScreen(params.title || "Native screen", color);
        } finally {
            context.uncoverApp();
        }
        return { seconds: (performance.now() - started) / 1000 };
    });

    context.register("pickFile", pickFile);

    // The page stands in for links that open the app: the hash of its address when it loads and every later change of the hash.
    const openUrl = () => {
        if (location.hash.length > 1) {
            context.emit("urlOpened", { url: location.href }, { retain: true });
        }
    };
    window.addEventListener("hashchange", openUrl);
    openUrl();

    // The error screen of the app shows this error. The plugin keeps it and hands it to the next app when that app sends start.
    context.onAppError((error) => {
        lastError = { message: error.message, file: error.file, line: error.line, language };
    });

    context.emit("loaded", { language, platform: "web" }, { retain: true });
}
