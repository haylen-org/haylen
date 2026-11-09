// Web part of the plugin of the plugin check, which answers with what the context of a web module gives a plugin: the features of the permissions policy that the page lacks, the failure of an error of the page, and the failure of a permission whose request ended without a prompt.
export default function load(context) {
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

    // A browser that shows its prompt only during an input of the person leaves the permission in the state "prompt" after a request.
    context.registerPermission({ name: "unprompted", capability: "the unprompted capability", state: () => "prompt", request: () => "prompt" });
    context.register("useUnprompted", async () => {
        await context.requirePermission("unprompted");
        return { used: true };
    });
}
