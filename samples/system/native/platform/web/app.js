// Page code of Haylen Native. The browser loads no native libraries, so the page answers the functions of the test library and the handlers of the sample through the bridge, and the Lua checks run against these answers.
Module.preRun.push(() => {
    const haylen = Module.haylen;
    const failure = (message, code, data) => Object.assign(new Error(message), { code, data });

    haylen.register("native_test.add", (params) => params.a + params.b);
    haylen.register("native_test.scale", (params) => params.value * params.factor);
    haylen.register("native_test.origin", () => "javascript");
    haylen.register("native_test.point_add", (params) => ({ x: params.a.x + params.b.x, y: params.a.y + params.b.y }));
    haylen.register("native_test.rect_grow", (params) => {
        const { rect, amount } = params;
        return { origin: { x: rect.origin.x - amount, y: rect.origin.y - amount }, width: rect.width + amount * 2, height: rect.height + amount * 2 };
    });
    haylen.register("native_test.fill", (params) => Array.from({ length: params.size }, (_, index) => (params.seed + index) % 256));
    haylen.register("native_test.checksum", (params) => params.bytes.reduce((hash, byte) => Math.imul(hash ^ byte, 16777619) >>> 0, 2166136261));

    // The report comes later, the way a thread of the library calls back.
    haylen.register("native_test.report_later", (params) => {
        setTimeout(() => haylen.emit("native_test.report", { value: params.value, data: [1, 2, 3, 4] }), 10);
        return null;
    });

    haylen.register("native_test.init", () => {
        setTimeout(() => haylen.emit("native_test.ready", { version: 1, origin: "javascript" }), 10);
        return true;
    });
    haylen.register("native_test.echo", (params) => new Promise((resolve) => setTimeout(() => resolve({ echo: params, thread: true }), 10)));
    haylen.register("native_test.fail", () => {
        throw failure("The native test failed on purpose.", "native_test_failure", { reason: "requested" });
    });
    haylen.register("native_test.wait", (params, context) => new Promise((resolve, reject) => {
        context.signal.addEventListener("abort", () => {
            haylen.emit("native_test.cancelled", { call: context.call });
            reject(context.signal.reason);
        });
    }));

    haylen.register("native_sample.greet", (params) => new Promise((resolve) => setTimeout(() => resolve({ greeting: "Hello, " + params.name, language: "JavaScript" }), 50)));
    haylen.register("native_sample.refuse", () => {
        throw failure("JavaScript refused on purpose.", "refused", { reason: "requested" });
    });
    haylen.register("native_sample.explode", () => {
        throw new TypeError("JavaScript threw on purpose.");
    });
    haylen.register("native_sample.slow", (params, context) => new Promise((resolve, reject) => {
        context.signal.addEventListener("abort", () => {
            haylen.emit("native_sample.cancelled", { language: "JavaScript" });
            reject(context.signal.reason);
        });
    }));
});
