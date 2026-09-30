// Native part of the platform sample on the web, in JavaScript. It answers platform-sample.echo and platform-sample.ticker, and sends platform-sample.activity when the tab shows or hides.
export default function load(context) {
    context.register("echo", (params) => {
        const text = typeof params.text === "string" ? params.text : "";
        if (!text) {
            throw new Error('The method "platform-sample.echo" needs a text.');
        }
        return { echo: text, characters: [...text].length, language: "JavaScript", system: navigator.userAgent };
    });

    context.register("ticker", (params) => {
        const count = Number.isInteger(params.count) && params.count > 0 ? params.count : 5;
        const interval = Number.isFinite(params.interval) && params.interval > 0 ? params.interval : 500;
        let sent = 0;
        const timer = setInterval(() => {
            sent += 1;
            context.emit("tick", { count: sent, total: count, source: "JavaScript" });
            if (sent === count) {
                clearInterval(timer);
            }
        }, interval);
        return { started: true, count, interval };
    });

    document.addEventListener("visibilitychange", () => {
        context.emit("activity", { state: document.visibilityState, source: "JavaScript" });
    });
}
