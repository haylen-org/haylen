// Stands in for the test library of the engine in the browser, which loads no native libraries: the page answers the functions of the library, its handlers and its events as methods and events of this plugin, and the Lua checks run against these answers.
export default function load(context) {
    const failure = (message, code, data) => Object.assign(new Error(message), { code, data });

    context.register("add", (params) => params.a + params.b);
    context.register("scale", (params) => params.value * params.factor);
    context.register("origin", () => "javascript");
    context.register("pointAdd", (params) => ({ x: params.a.x + params.b.x, y: params.a.y + params.b.y }));
    context.register("rectGrow", (params) => {
        const { rect, amount } = params;
        return { origin: { x: rect.origin.x - amount, y: rect.origin.y - amount }, width: rect.width + amount * 2, height: rect.height + amount * 2 };
    });
    context.register("fill", (params) => Array.from({ length: params.size }, (_, index) => (params.seed + index) % 256));
    context.register("checksum", (params) => params.bytes.reduce((hash, byte) => Math.imul(hash ^ byte, 16777619) >>> 0, 2166136261));

    // The report comes later, the way a thread of the library calls back.
    context.register("reportLater", (params) => {
        setTimeout(() => context.emit("report", { value: params.value, data: [1, 2, 3, 4] }), 10);
        return null;
    });

    context.register("init", () => {
        setTimeout(() => context.emit("ready", { version: 1, origin: "javascript" }), 10);
        return true;
    });
    context.register("echo", (params) => new Promise((resolve) => setTimeout(() => resolve({ echo: params, thread: true }), 10)));
    context.register("fail", () => {
        throw failure("The native test failed on purpose.", "native_test_failure", { reason: "requested" });
    });
    context.register("wait", (params, call) => new Promise((resolve, reject) => {
        call.signal.addEventListener("abort", () => {
            context.emit("cancelled", { call: call.call });
            reject(call.signal.reason);
        });
    }));
}
