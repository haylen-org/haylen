// Native part of the native sample on the web: a greeting, a typed refusal, a handler that throws and a slow call that reports its cancellation.
export default function load(context) {
    const failure = (message, code, data) => Object.assign(new Error(message), { code, data });

    context.register("greet", (params) => new Promise((resolve) => setTimeout(() => resolve({ greeting: "Hello, " + params.name, language: "JavaScript" }), 50)));
    context.register("refuse", () => {
        throw failure("JavaScript refused on purpose.", "refused", { reason: "requested" });
    });
    context.register("explode", () => {
        throw new TypeError("JavaScript threw on purpose.");
    });
    context.register("slow", (params, call) => new Promise((resolve, reject) => {
        call.signal.addEventListener("abort", () => {
            context.emit("cancelled", { language: "JavaScript" });
            reject(call.signal.reason);
        });
    }));
}
