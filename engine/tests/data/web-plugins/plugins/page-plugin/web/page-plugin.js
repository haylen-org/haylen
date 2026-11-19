// Web part of the plugin of the plugin check, which answers with what the context of a web module gives a plugin: the features of the permissions policy that the page lacks, the failure of an error of the page, the failure of a result that JSON cannot carry, the failure of a permission whose request ended without a prompt, the failure of a permission that the plugin never registered and the events that it sends before the app starts.
export default function load(context) {
    // The page keeps the newest events of each name until the first app starts, so the app hears 32 of these 40. They reach the engine in one frame as one retained batch, which waits for the first listener however late the scripts of the app start.
    for (let index = 0; index < 40; ++index) {
        context.emit("sample", { index }, { retain: true, batched: true });
    }

    context.register("check", (params) => {
        try {
            context.require({ permissionsPolicy: params.features });
        } catch (error) {
            return { missing: error.data.missing.map((entry) => entry.name) };
        }

        return { missing: [] };
    });

    // An error of the page, such as the refusal of a browser API, carries a number as its code, which is no code of a failure.
    context.register("fail", () => {
        throw new DOMException("The browser refused the request.", "NotAllowedError");
    });

    // JSON has no form for a `BigInt`, so the call fails with the error of its encoding.
    context.register("unwritable", () => ({ count: 1n }));

    // A browser that shows its prompt only during an input of the person leaves the permission in the state "prompt" after a request.
    context.registerPermission({ name: "unprompted", capability: "the unprompted capability", state: () => "prompt", request: () => "prompt" });
    context.register("useUnprompted", async () => {
        await context.requirePermission("unprompted");
        return { used: true };
    });

    context.register("useUnregistered", async () => {
        await context.requirePermission("unregistered");
        return { used: true };
    });
}
