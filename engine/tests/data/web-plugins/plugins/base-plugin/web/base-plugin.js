// Web part of the plugin that the plugin of the plugin check depends on, which that plugin imports as "haylen-plugin:base-plugin". Both share one instance of this module, so the count of its calls is the same for them.
let calls = 0;

export function describe(text) {
    calls += 1;
    return text + ", as the web module of base-plugin wrote it for the call " + calls + ".";
}

export default function load() {}
