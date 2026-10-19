// The banner of the {{TITLE}} plugin on the web: a bar with a text and a button, which the overlay of the plugin places over the canvas at its top or bottom edge, and whose button sends the event `bannerTapped`. Clicks outside the bar reach the app.
const failure = (message, code) => Object.assign(new Error(message), { code });

export default function registerBanner(context) {
    let banner = null;
    let taps = 0;

    context.register("showBanner", (params) => {
        if (params.anchor !== "top" && params.anchor !== "bottom") {
            throw failure(`The anchor of the banner is "top" or "bottom", not "${params.anchor}".`, "invalidAnchor");
        }
        const bar = document.createElement("div");
        bar.style.cssText = "display:flex;align-items:center;gap:12px;padding:0 8px 0 16px;border-radius:10px;color:#fff;font:600 16px system-ui,sans-serif;background:#5856d6;";
        const label = document.createElement("span");
        label.textContent = params.text;
        label.style.cssText = "flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;";
        const button = document.createElement("button");
        button.textContent = "Tap";
        button.style.cssText = "font:600 15px system-ui,sans-serif;padding:8px 16px;border:0;border-radius:8px;background:#fff;color:#3a38a8;cursor:pointer;";
        button.addEventListener("click", () => {
            taps += 1;
            context.emit("bannerTapped", { count: taps });
        });
        // The button leaves the focus, and so the keyboard, with the app.
        button.addEventListener("mousedown", (event) => event.preventDefault());
        bar.append(label, button);
        banner?.remove();
        banner = context.overlay.add(bar, { anchor: params.anchor, reserve: params.reserve, width: 360, height: 56 });
        return null;
    });

    context.register("hideBanner", () => {
        banner?.remove();
        banner = null;
        return null;
    });
}
