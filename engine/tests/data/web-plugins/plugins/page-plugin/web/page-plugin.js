// Web part of the plugin of the plugin check, which answers with what the context of a web module gives a plugin: the features of the permissions policy that the page lacks, the failure of an error of the page, the failure of a result that JSON cannot carry, the text of the web module of the plugin it depends on and the events that it sends before the app starts.
import { describe } from "haylen-plugin:base-plugin";

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

    // The module of the plugin it depends on reaches it through the import map of the page.
    context.register("dependency", (params) => ({ text: describe(params.text) }));
}
