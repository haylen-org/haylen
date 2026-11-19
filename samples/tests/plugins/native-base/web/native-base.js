// Native part of the plugin `native-base` on the web. Its helper `describe` is a named export, which the web modules of the plugins that depend on it import from `haylen-plugin:native-base`, so they share this one instance of the module, and its method answers the app with the same text.
export const describe = (text) => `${text}, as the native part of native-base wrote it on the web.`;

export default function load(context) {
    context.register("describe", (params) => ({ text: describe(params.text), language: "JavaScript" }));
}
