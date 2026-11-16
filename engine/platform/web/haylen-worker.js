// The dedicated worker of the Worker mode of the Haylen web runtime. The page side of `haylen-page.js` starts it from next to the runtime script and posts it the canvas as an `OffscreenCanvas`, the options of the page, the bytes of the WebAssembly module that the page downloaded, if any, and the state of the page, and it loads the runtime script, whose engine side of `haylen-runtime.js` answers the page from then on.

self.onmessage = (event) => {
    const message = event.data;
    if (message.type !== "start") {
        throw new Error("The worker of the runtime received the message \"" + message.type + "\" before it started.");
    }

    const { wasmBinary, ...options } = message.options;
    self.Module = { canvas: message.canvas, wasmBinary, haylen: { ...options, hosting: "worker", state: message.state } };
    importScripts(message.script);
};
