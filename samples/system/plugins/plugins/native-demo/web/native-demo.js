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

// The pattern of the demo on a canvas: a gradient from red to green with blue stripes that move with the frame.
const drawPattern = (canvas, frame) => {
    const context = canvas.getContext("2d");
    const gradient = context.createLinearGradient(0, 0, canvas.width, canvas.height);
    gradient.addColorStop(0, "#ff0000");
    gradient.addColorStop(1, "#00ff00");
    context.fillStyle = gradient;
    context.fillRect(0, 0, canvas.width, canvas.height);
    context.fillStyle = "rgba(0, 0, 230, 0.6)";
    for (let offset = -canvas.height - 32 + ((frame * 4) % 32); offset < canvas.width; offset += 32) {
        context.beginPath();
        context.moveTo(offset, 0);
        context.lineTo(offset + 16, 0);
        context.lineTo(offset + 16 + canvas.height, canvas.height);
        context.lineTo(offset + canvas.height, canvas.height);
        context.fill();
    }
};

// Draws the pattern on a canvas and encodes it as a PNG file with toBlob.
const generatedImage = (width, height) =>
    new Promise((resolve, reject) => {
        if (!(width >= 1 && height >= 1 && width <= 2048 && height <= 2048)) {
            reject(failure('The method "generatedImage" needs a width and a height from 1 to 2048.', "invalidParams"));
            return;
        }
        const canvas = document.createElement("canvas");
        canvas.width = width;
        canvas.height = height;
        drawPattern(canvas, 0);
        canvas.toBlob(async (blob) => resolve({ png: new Uint8Array(await blob.arrayBuffer()), width, height, drawnWith: "a canvas and toBlob", language }), "image/png");
    });

const bannerColor = (config) => {
    if (!/^#[0-9A-Fa-f]{6}$/.test(config.bannerColor)) {
        throw failure('The parameter "bannerColor" must be a color as "#RRGGBB", not "' + config.bannerColor + '".', "invalidColor");
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

// The confirm screen of the plugin, a page next to this module that shows in a popup or replaces the page of the app for a redirect.
const screenPage = (mode, params, screen, extra) => {
    const page = new URL("screen.html", import.meta.url);
    page.search = new URLSearchParams({ mode, token: screen.token, title: params.title || "Native Demo", question: params.question || "", ...extra }).toString();
    return page.href;
};

// A page that comes back from the redirect screen carries the answer and the token of the screen in its address, which end the screen that the redirect left. The address loses both, so a reload shows the app alone.
const finishRedirect = (screen) => {
    const address = new URL(location.href);
    const answer = address.searchParams.get("nativeDemoAnswer");
    if (answer !== null && address.searchParams.get("nativeDemoScreen") === screen.token) {
        screen.resolve({ confirmed: answer === "confirmed", via: "redirect", language });
    } else {
        screen.reject(failure("The page came back without the answer of the screen.", "cancelled"));
    }
    address.searchParams.delete("nativeDemoAnswer");
    address.searchParams.delete("nativeDemoScreen");
    history.replaceState(history.state, "", address.href);
};

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
    const video = { stream: context.videoStream("pattern"), canvas: null, timer: null, frame: 0 };
    const tone = { stream: context.audioStream("tone", { sampleRate: 44100, channels: 1 }), timer: null };
    let bursts = null;
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

    const stopVideo = () => {
        clearInterval(video.timer);
        video.timer = null;
    };

    const stopTone = () => {
        clearInterval(tone.timer);
        tone.timer = null;
    };

    const stopBursts = () => {
        clearInterval(bursts);
        bursts = null;
    };

    context.register("echo", (params) => ({ echo: params.value === undefined ? null : params.value, thread: "main", language }));

    // The bytes of the app arrive as a Uint8Array, which the answer carries back as bytes.
    context.register("echoBytes", (params) => {
        if (!(params.data instanceof Uint8Array)) {
            throw failure('The method "echoBytes" needs bytes.', "invalidParams");
        }
        return { data: params.data, size: params.data.length, thread: "main", language };
    });

    context.register("generatedImage", (params) => generatedImage(params.width, params.height));

    // A canvas animates the pattern 30 times per second, and the video stream copies each frame into the texture of the app.
    context.register("startVideo", () => {
        stopVideo();
        video.canvas = video.canvas || Object.assign(document.createElement("canvas"), { width: 320, height: 180 });
        video.timer = setInterval(() => {
            drawPattern(video.canvas, video.frame++);
            video.stream.push(video.canvas);
        }, 1000 / 30);
        return { width: video.canvas.width, height: video.canvas.height, fps: 30, format: "RGBA", thread: "main", language };
    });

    context.register("stopVideo", () => {
        stopVideo();
        return null;
    });

    // Synthesizes a sine wave a tenth of a second ahead of the clock of the page, which the audio stream copies into its ring.
    context.register("startTone", (params) => {
        stopTone();
        const frequency = params.frequency || 440;
        const started = performance.now();
        let phase = 0;
        let written = 0;
        tone.timer = setInterval(() => {
            const target = ((performance.now() - started) / 1000 + 0.1) * 44100;
            while (written < target) {
                const samples = new Float32Array(441);
                for (let index = 0; index < samples.length; ++index) {
                    samples[index] = Math.sin(phase) * 0.3;
                    phase = (phase + (2 * Math.PI * frequency) / 44100) % (2 * Math.PI);
                }
                tone.stream.push(samples);
                written += samples.length;
            }
        }, 10);
        return { frequency, sampleRate: 44100, channels: 1, format: "float32", language };
    });

    context.register("stopTone", () => {
        stopTone();
        return null;
    });

    // Sends count batched events 30 times per second for ticks ticks, which reach the app as one list per frame, and then burstDone.
    context.register("burst", (params) => {
        stopBursts();
        let tick = 0;
        bursts = setInterval(() => {
            for (let index = 0; index < params.count; ++index) {
                context.emit("burst", { tick, index, language }, { batched: true });
            }
            tick += 1;
            if (tick === params.ticks) {
                stopBursts();
                context.emit("burstDone", { events: params.count * params.ticks, ticks: params.ticks, language });
            }
        }, 1000 / 30);
        return { count: params.count, ticks: params.ticks, language };
    });

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
        stopVideo();
        stopTone();
        stopBursts();
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
            throw failure('The anchor of the banner is "top" or "bottom", not "' + params.anchor + '".', "invalidAnchor");
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
            throw failure('No banner shows. Call "showBanner" first.', "noBanner");
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

    // The plugin needs the Contact Picker API, which only browsers on phones offer, so the call shows how a requirement that the page lacks fails with the code `unsupported` and lists what is missing in `data.missing`. A browser that offers the API in a secure context gets the answer.
    context.register("requirementCheck", () => {
        context.require({ secureContext: true, api: "navigator.contacts" });
        return { met: true, language };
    });

    // The confirm screen opens in a popup inside the activation of the tap that asked for it, and answers with what the person picked there, which the popup posts back to this page.
    context.registerScreen("confirm", async (params, screen) => {
        const answer = await screen.popup(screenPage("popup", params, screen), { width: 440, height: 420 });
        screen.resolve({ confirmed: answer.confirmed, via: "popup", language });
    });

    // The redirect screen leaves this page for the confirm page, which comes back with the answer in the address, so the page loads again and the answer reaches the new app as screenRestored.
    context.registerScreen("redirect", (params, screen) => {
        screen.redirect(screenPage("redirect", params, screen, { back: location.href }));
    });

    if (context.restoredScreen) {
        finishRedirect(context.restoredScreen);
    }

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
