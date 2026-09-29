// Page code of Haylen Platform, which answers the bridge methods of the sample and sends its events from JavaScript.
// sample.echo answers with the text it received, sample.ticker sends sample.tick events for a while, and sample.activity follows the visibility of the tab.
Module.preRun.push(() => {
    Module.haylen.register("sample.echo", (params) => {
        const text = typeof params.text === "string" ? params.text : "";
        if (!text) {
            throw new Error("sample.echo needs a text.");
        }
        return { echo: text, characters: [...text].length, language: "JavaScript", system: navigator.userAgent };
    });

    Module.haylen.register("sample.ticker", (params) => {
        const count = Number.isInteger(params.count) && params.count > 0 ? params.count : 5;
        const interval = Number.isFinite(params.interval) && params.interval > 0 ? params.interval : 500;
        let sent = 0;
        const timer = setInterval(() => {
            sent += 1;
            Module.haylen.emit("sample.tick", { count: sent, total: count, source: "JavaScript" });
            if (sent === count) {
                clearInterval(timer);
            }
        }, interval);
        return { started: true, count, interval };
    });

    document.addEventListener("visibilitychange", () => {
        Module.haylen.emit("sample.activity", { state: document.visibilityState, source: "JavaScript" });
    });
});
