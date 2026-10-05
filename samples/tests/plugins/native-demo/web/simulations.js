// The simulations of SDKs on the web, built on the APIs of the browser alone: the camera of `getUserMedia` into the video stream `camera` with photos as JPEG bytes, the microphone into the audio stream `microphone` with its level, the Geolocation API, a drawn map over the canvas, the Web Share API, other apps through their links, and a fake store, fake ads and a fake sign-in whose dialogs are screens of the plugin. A browser without an API, or a page outside a secure context, fails the call with the code `unsupported` and the list of what is missing.
const language = "JavaScript";

const failure = (message, code, data) => Object.assign(new Error(message), { code, data });

const products = [
    { id: "coins.small", title: "A pouch of coins", description: "100 coins for the shop of the app.", price: "0.99", currency: "USD", kind: "consumable" },
    { id: "coins.large", title: "A chest of coins", description: "1200 coins for the shop of the app.", price: "9.99", currency: "USD", kind: "consumable" },
    { id: "ads.remove", title: "No more ads", description: "Removes the ads of the demo for good.", price: "2.99", currency: "USD", kind: "nonConsumable" },
];
const accounts = [
    { userId: "demo-ana", name: "Ana Souza", email: "ana@example.com" },
    { userId: "demo-bruno", name: "Bruno Lima", email: "bruno@example.com" },
];
const purchasesKey = "native-demo.purchases";
// Transactions and tokens need only to differ from each other, so they come from the clock and a random number, which pages outside a secure context have too.
const newId = () => Date.now().toString(36) + "-" + Math.random().toString(36).slice(2, 10);
const accountKey = "native-demo.account";

// The local storage of the page keeps the purchases and the account, and a page without it, such as one with blocked storage, keeps them while it lives.
const saved = (() => {
    const memory = new Map();
    return {
        get(key) {
            try {
                return JSON.parse(localStorage.getItem(key));
            } catch {
                return memory.has(key) ? memory.get(key) : null;
            }
        },
        set(key, value) {
            memory.set(key, value);
            try {
                if (value === null) {
                    localStorage.removeItem(key);
                } else {
                    localStorage.setItem(key, JSON.stringify(value));
                }
            } catch {
                return;
            }
        },
    };
})();

// A dialog over the canvas with a title, a message and a button for each choice, the way the sheets of SDKs ask. It resolves with the index of the choice, or `null` for the choice that cancels and for Escape, and the screen that the app gives up closes it.
const ask = (screen, title, message, choices, cancel) =>
    new Promise((resolve) => {
        const dialog = document.createElement("dialog");
        dialog.style.cssText = "max-width:420px;border:0;border-radius:14px;padding:24px;font:16px system-ui,sans-serif;color:#1b1e2b;background:#fff;box-shadow:0 12px 40px rgba(0,0,0,0.4);";
        const heading = document.createElement("h2");
        heading.textContent = title;
        heading.style.cssText = "margin:0 0 8px;font:700 20px system-ui,sans-serif;";
        const text = document.createElement("p");
        text.textContent = message;
        text.style.margin = "0 0 20px";
        const buttons = document.createElement("div");
        buttons.style.cssText = "display:flex;flex-direction:column;gap:8px;";
        let answer = null;
        const add = (label, index, primary) => {
            const button = document.createElement("button");
            button.textContent = label;
            button.style.cssText = "font:600 15px system-ui,sans-serif;padding:10px 16px;border:0;border-radius:8px;cursor:pointer;" + (primary ? "background:#4c7dff;color:#fff;" : "background:#e6e8ef;color:#1b1e2b;");
            button.addEventListener("click", () => {
                answer = index;
                dialog.close();
            });
            buttons.append(button);
            return button;
        };
        const first = choices.map((choice, index) => add(choice, index, true))[0];
        add(cancel, null, false);
        dialog.append(heading, text, buttons);
        dialog.addEventListener("close", () => {
            dialog.remove();
            resolve(answer);
        });
        screen.signal.addEventListener("abort", () => dialog.close());
        document.body.append(dialog);
        dialog.showModal();
        first.focus();
    });

// The map view of the demo, which draws what a view of a map SDK shows without the SDK or the network: the sea, a coast, a grid, roads and a pin on the place with its coordinates.
const drawMap = (latitude, longitude) => {
    const scale = window.devicePixelRatio || 1;
    const canvas = document.createElement("canvas");
    canvas.width = 360 * scale;
    canvas.height = 220 * scale;
    canvas.style.cssText = "width:360px;height:220px;border-radius:12px;";
    const context = canvas.getContext("2d");
    context.scale(scale, scale);
    context.fillStyle = "#aad3df";
    context.fillRect(0, 0, 360, 220);
    const wave = (latitude - Math.floor(latitude)) * 55;
    context.fillStyle = "#e8e1ca";
    context.beginPath();
    context.moveTo(0, 77 + wave);
    context.bezierCurveTo(108, 33, 216, 132 - wave, 360, 66);
    context.lineTo(360, 220);
    context.lineTo(0, 220);
    context.fill();
    context.strokeStyle = "rgba(0,0,0,0.24)";
    context.lineWidth = 1;
    for (let step = 1; step < 6; ++step) {
        context.beginPath();
        context.moveTo((360 * step) / 6, 0);
        context.lineTo((360 * step) / 6, 220);
        context.moveTo(0, (220 * step) / 6);
        context.lineTo(360, (220 * step) / 6);
        context.stroke();
    }
    context.strokeStyle = "#fff";
    context.lineWidth = 4;
    context.beginPath();
    context.moveTo(0, 165);
    context.lineTo(360, 121);
    context.moveTo(216, 220);
    context.lineTo(162, 88);
    context.stroke();
    context.fillStyle = "#db4437";
    context.beginPath();
    context.arc(180, 100, 9, 0, Math.PI * 2);
    context.fill();
    context.beginPath();
    context.moveTo(172, 104);
    context.lineTo(188, 104);
    context.lineTo(180, 118);
    context.fill();
    context.fillStyle = "#fff";
    context.beginPath();
    context.arc(180, 100, 3.5, 0, Math.PI * 2);
    context.fill();
    context.fillStyle = "#202124";
    context.font = "12px system-ui,sans-serif";
    context.fillText(latitude.toFixed(4) + ", " + longitude.toFixed(4), 10, 210);
    return canvas;
};

export default function simulate(context) {
    const camera = { stream: context.videoStream("camera"), media: null, video: null, timer: null };
    const microphone = { stream: null, media: null, audio: null };
    let map = null;

    const stopCamera = () => {
        clearInterval(camera.timer);
        camera.timer = null;
        camera.media?.getTracks().forEach((track) => track.stop());
        camera.media = null;
    };

    const stopMicrophone = () => {
        microphone.media?.getTracks().forEach((track) => track.stop());
        microphone.media = null;
        microphone.audio?.close();
        microphone.audio = null;
    };

    const removeMap = () => {
        map?.remove();
        map = null;
    };

    // The camera plays in a video element of its own, which a timer copies into the stream 30 times per second.
    context.register("startCamera", async (params) => {
        context.require({ secureContext: true, api: "navigator.mediaDevices.getUserMedia", permissionsPolicy: "camera" });
        stopCamera();
        try {
            camera.media = await navigator.mediaDevices.getUserMedia({ video: { width: 640, height: 480, facingMode: params.facing === "front" ? "user" : "environment" } });
        } catch (error) {
            throw failure("The camera did not start: " + error.message, error.name === "NotAllowedError" ? "permissionDenied" : "unsupported");
        }
        camera.video = Object.assign(document.createElement("video"), { muted: true, playsInline: true, srcObject: camera.media });
        await camera.video.play();
        camera.timer = setInterval(() => camera.stream.push(camera.video), 1000 / 30);
        return { width: camera.video.videoWidth, height: camera.video.videoHeight, facing: params.facing || "back", language };
    });

    context.register("stopCamera", () => {
        stopCamera();
        return null;
    });

    context.register("takePhoto", async () => {
        if (!camera.timer) {
            throw failure('The camera is off. Call "startCamera" first.', "cameraOff");
        }
        const canvas = Object.assign(document.createElement("canvas"), { width: camera.video.videoWidth, height: camera.video.videoHeight });
        canvas.getContext("2d").drawImage(camera.video, 0, 0);
        const blob = await new Promise((resolve) => canvas.toBlob(resolve, "image/jpeg", 0.8));
        return { jpeg: new Uint8Array(await blob.arrayBuffer()), width: canvas.width, height: canvas.height, language };
    });

    // The microphone plays into an audio worklet whose processor posts every block of samples to the page, which pushes them into the stream and measures their level about ten times per second.
    context.register("startMicrophone", async () => {
        context.require({ secureContext: true, api: ["navigator.mediaDevices.getUserMedia", "AudioWorkletNode"], permissionsPolicy: "microphone" });
        stopMicrophone();
        try {
            microphone.media = await navigator.mediaDevices.getUserMedia({ audio: true });
        } catch (error) {
            throw failure("The microphone did not start: " + error.message, error.name === "NotAllowedError" ? "permissionDenied" : "unsupported");
        }
        microphone.audio = new AudioContext();
        const sampleRate = microphone.audio.sampleRate;
        microphone.stream = context.audioStream("microphone", { sampleRate, channels: 1 });
        const processor = "registerProcessor('native-demo-microphone', class extends AudioWorkletProcessor { process(inputs) { if (inputs[0][0]) { this.port.postMessage(inputs[0][0].slice()); } return true; } });";
        await microphone.audio.audioWorklet.addModule(URL.createObjectURL(new Blob([processor], { type: "text/javascript" })));
        const node = new AudioWorkletNode(microphone.audio, "native-demo-microphone");
        let squares = 0;
        let measured = 0;
        node.port.onmessage = ({ data }) => {
            microphone.stream.push(data);
            for (const sample of data) {
                squares += sample * sample;
            }
            measured += data.length;
            if (measured >= sampleRate / 10) {
                context.emit("microphoneLevel", { level: Math.sqrt(squares / measured), language });
                squares = 0;
                measured = 0;
            }
        };
        microphone.audio.createMediaStreamSource(microphone.media).connect(node);
        return { sampleRate, channels: 1, language };
    });

    context.register("stopMicrophone", () => {
        stopMicrophone();
        return null;
    });

    context.register("location", () => {
        context.require({ secureContext: true, api: "navigator.geolocation", permissionsPolicy: "geolocation" });
        return new Promise((resolve, reject) => {
            navigator.geolocation.getCurrentPosition(
                (position) => resolve({ latitude: position.coords.latitude, longitude: position.coords.longitude, accuracy: position.coords.accuracy, language }),
                (error) => reject(failure("The location is unknown: " + error.message, error.code === error.PERMISSION_DENIED ? "permissionDenied" : "locationUnknown")),
                { timeout: 15000 },
            );
        });
    });

    context.register("showMap", (params) => {
        if (params.anchor !== "top" && params.anchor !== "bottom") {
            throw failure('The anchor of the map is "top" or "bottom", not "' + params.anchor + '".', "invalidAnchor");
        }
        removeMap();
        map = context.overlay.add(drawMap(params.latitude, params.longitude), { anchor: params.anchor, width: 360, height: 220, margin: 16 });
        return { anchor: params.anchor, latitude: params.latitude, longitude: params.longitude, drawnWith: "a canvas of the page", language };
    });

    context.register("removeMap", () => {
        removeMap();
        return null;
    });

    // The share sheet of the browser opens only right after a click, a tap or a key press, and tells whether the person shared.
    context.register("share", async (params) => {
        context.require({ secureContext: true, api: "navigator.share" });
        try {
            await navigator.share(params.url ? { text: params.text, url: params.url } : { text: params.text });
            return { shared: true, language };
        } catch (error) {
            if (error.name === "AbortError") {
                return { shared: false, language };
            }
            throw failure("The share sheet did not open: " + error.message, error.name === "NotAllowedError" ? "noUserGesture" : "unsupported");
        }
    });

    // A page reaches the maps and the mail of the system through their links, and has no link to the settings of the browser.
    context.register("openApp", (params) => {
        const links = { maps: "geo:-22.9519,-43.2105", mail: "mailto:demo@example.com?subject=Native%20Demo" };
        if (params.kind === "settings") {
            throw failure("A web page cannot open the settings of the browser or of the system.", "unsupported", { language });
        }
        if (!links[params.kind]) {
            throw failure('The app to open is "settings", "maps" or "mail", not "' + params.kind + '".', "invalidApp");
        }
        const link = Object.assign(document.createElement("a"), { href: links[params.kind], target: "_blank", rel: "noopener" });
        link.click();
        return { opened: true, language };
    });

    context.register("products", () => products);

    context.register("restorePurchases", () => {
        const kept = saved.get(purchasesKey) || [];
        for (const purchase of kept) {
            context.emit("purchaseUpdated", { ...purchase, state: "restored", language });
        }
        return kept;
    });

    context.register("currentUser", () => saved.get(accountKey));

    context.register("signOut", () => {
        saved.set(accountKey, null);
        context.emit("userChanged", null);
        return null;
    });

    context.registerScreen("purchase", async (params, screen) => {
        const product = products.find((item) => item.id === params.productId);
        if (!product) {
            throw failure('The store has no product "' + params.productId + '".', "unknownProduct");
        }
        const picked = await ask(screen, product.title, product.description + " This purchase is a simulation, and nothing is charged.", ["Buy for " + product.price + " " + product.currency], "Cancel");
        if (picked === null) {
            throw failure("The person cancelled the purchase.", "cancelled");
        }
        const purchase = { productId: product.id, transactionId: newId() };
        if (product.kind === "nonConsumable") {
            saved.set(purchasesKey, [...(saved.get(purchasesKey) || []).filter((kept) => kept.productId !== product.id), purchase]);
        }
        context.emit("purchaseUpdated", { ...purchase, state: "purchased", language });
        screen.resolve({ ...purchase, receipt: btoa(JSON.stringify(purchase)), language });
    });

    context.registerScreen("interstitial", async (params, screen) => {
        await ask(screen, "Demo ad", "A fake full-screen ad of the demo plugin, which covers the app until it closes.", ["Continue to the app"], "Close");
        screen.resolve({ closed: true, language });
    });

    context.registerScreen("rewarded", async (params, screen) => {
        const rewarded = (await ask(screen, "Demo rewarded ad", "Watch this fake ad to the end to earn 50 coins.", ["Watch to the end"], "Skip")) !== null;
        if (rewarded) {
            context.emit("adRewarded", { amount: 50, currency: "coins", language });
        }
        screen.resolve({ rewarded, amount: rewarded ? 50 : 0, currency: "coins", language });
    });

    context.registerScreen("signIn", async (params, screen) => {
        const picked = await ask(screen, "Sign in to the demo", "Pick a fake account. No password and no network take part.", accounts.map((account) => account.name + ", " + account.email), "Cancel");
        if (picked === null) {
            throw failure("The person cancelled the sign-in.", "cancelled");
        }
        const account = { ...accounts[picked], token: newId(), language };
        saved.set(accountKey, account);
        context.emit("userChanged", account);
        screen.resolve(account);
    });

    // Ends what an earlier app of the page left running.
    return () => {
        stopCamera();
        stopMicrophone();
        removeMap();
    };
}
