// Web part of the plugin of the plugin check, which answers with what the context of a web module gives a plugin: the features of the permissions policy that the page lacks, and the failure of an error of the page.
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
}
