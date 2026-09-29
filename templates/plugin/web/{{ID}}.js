// Web part of the {{TITLE}} plugin. The page imports this module before the runtime starts and calls its load function with the context of the plugin.
export default function load(context) {
    // Answers {{ID}}.echo with the message it receives, and sends it to the app again as the {{ID}}.echoed event.
    context.register("echo", (params) => {
        context.emit("echoed", { message: params.message });
        return { message: params.message };
    });
}
