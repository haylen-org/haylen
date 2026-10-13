// The page code of the app, which runs before the runtime starts: the page of a Lua app loads it after `loader.js`, and a C++ app links it into its runtime script with `--pre-js`.
// The platform bridge handlers of the app register here with `Module.haylen.register` in `Module.preRun`, and page code sends events to the app with `Module.haylen.emit` once the runtime started.
let greetings = 0;

Module.preRun = Module.preRun ?? [];
Module.preRun.push(() => {
    // Answers "app.greet" with a greeting for the name that the app sends, and sends "app.greeted" with the count of greetings so far.
    Module.haylen.register("app.greet", (params) => {
        greetings += 1;
        Module.haylen.emit("app.greeted", { count: greetings });
        return { message: `Hello, ${params.name}, from JavaScript.` };
    });
});
