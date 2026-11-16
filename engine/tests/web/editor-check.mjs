import { readFileSync } from "node:fs";

// Opens the page of the editor check in a headless browser over its debugging protocol and drives the editor contract of the generic web player: it builds a project file by file, runs it, reloads a module, rejects a syntax error, restores the code that runs, applies a failing batch atomically, stops code that never ends through the execution budget while the page answers, inspects the app, runs it again in place while its audio output opens, stops it early and disposes of the runtime. It prints what happened as JSON, with every error that the page logged, since the audio output must open and close again without one.
// In the Worker mode, with the mode "worker" and a page address that asks for it, the runtime runs in a dedicated worker: `hello` reports it, an audio context that the browser refuses leaves the app without an output, a key and a click of the browser reach the app through the page, a controller that the page reports reaches the app, the mirror of screen readers shows a button of the app and activates it, the web module of a plugin answers a call, opens a screen and pushes a video and an audio stream, a video of the package plays, and on a second load of the page the editor terminates a worker that a finalizer holds while the page and its heartbeat tell it stopped answering.
// Run it with Node: node editor-check.mjs <debugging port> <page address> [worker]
const [port, url, mode] = process.argv.slice(2);
const worker = mode === "worker";
const version = await (await fetch(`http://127.0.0.1:${port}/json/version`)).json();
const socket = new WebSocket(version.webSocketDebuggerUrl);
const pending = new Map();
const result = { steps: [], failure: null, log: [] };
let id = 0;

const send = (method, params = {}, sessionId = undefined) =>
    new Promise((resolve) => {
        id += 1;
        pending.set(id, resolve);
        socket.send(JSON.stringify(sessionId ? { id, method, params, sessionId } : { id, method, params }));
    });

socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    // The runtime of the Worker mode logs in its worker, whose console the check follows like the console of the page.
    if (message.method === "Target.attachedToTarget" && message.params.targetInfo.type === "worker") {
        send("Runtime.enable", {}, message.params.sessionId);
    }
    if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message.result ?? { exceptionDetails: message.error });
        pending.delete(message.id);
    } else if (message.method === "Runtime.consoleAPICalled") {
        const text = message.params.args.map((arg) => arg.value ?? arg.description ?? "").join(" ");
        result.log.push(text);
        // The log of the engine reaches the console as lines that start with their time, and every other error of the page is a failure, such as one of the audio output.
        if (message.params.type === "error" && !text.startsWith("[") && !result.failure) {
            result.failure = "The page logged an error: " + text;
        }
    } else if (message.method === "Runtime.exceptionThrown" && !result.failure) {
        result.failure = message.params.exceptionDetails.exception?.description ?? message.params.exceptionDetails.text;
    } else if (message.method === "Page.loadEventFired" && loaded) {
        loaded();
    }
};

// Loads the page and waits for its load event within a bound, so no step runs in the page that the navigation replaces.
let loaded = null;
const navigate = async () => {
    const load = new Promise((resolve) => (loaded = resolve));
    await send("Page.navigate", { url }, sessionId);
    await Promise.race([load, new Promise((resolve) => setTimeout(resolve, 30000))]);
    loaded = null;
};

await new Promise((resolve) => (socket.onopen = resolve));
const page = (await send("Target.getTargets")).targetInfos.find((target) => target.type === "page");
const { sessionId } = await send("Target.attachToTarget", { targetId: page.targetId, flatten: true });
await send("Runtime.enable", {}, sessionId);
await send("Target.setAutoAttach", { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, sessionId);
await send("Page.enable", {}, sessionId);
await navigate();

// Runs a script of the page that answers whether the step held, within the seconds.
const step = async (name, script, seconds) => {
    const evaluation = send("Runtime.evaluate", { expression: `(async () => { ${script} })()`, awaitPromise: true, returnByValue: true }, sessionId);
    const timeout = new Promise((resolve) => setTimeout(() => resolve({ timedOut: true }), seconds * 1000));
    const answer = await Promise.race([evaluation, timeout]);
    const reached = answer.result?.value === true;
    result.steps.push({ name, reached });
    if (!reached && !result.failure) {
        result.failure = answer.timedOut ? `The step "${name}" took longer than ${seconds} seconds.` : `The step "${name}" did not hold: ${JSON.stringify(answer.result?.value ?? answer.exceptionDetails ?? null)}`;
    }
    return reached;
};

const readyScript = `for (let tries = 0; tries < 300 && !(window.Module && Module.haylen && Module.haylen.editor); ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); } const hello = await Module.haylen.editor.request('hello'); return hello.protocol === 1 && hello.capabilities.packageEditing && hello.capabilities.developmentReload && hello.capabilities.workerHosting === ${worker};`;
const ready = await step("ready", readyScript, 40);
if (ready && worker) {
    await step("refused an audio output", "const original = window.AudioContext; window.AudioContext = function () { throw new Error('The check refuses the audio context.'); }; await editor.request('clearFiles'); await editor.request('edit', { operations: [{ op: 'write', path: 'app.json', content: check.app }, { op: 'write', path: 'source/main.lua', content: tone }] }); await editor.request('run'); let state = ''; for (let tries = 0; tries < 30 && state !== 'unavailable'; ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); state = (await editor.request('console', { line: \"lua require('haylen.audio').outputState()\" })).output.trim(); } window.AudioContext = original; await editor.request('stop'); return state === 'unavailable';", 20);
    await step("prepared input", "await editor.request('clearFiles'); await editor.request('edit', { operations: [{ op: 'write', path: 'app.json', content: check.app }, { op: 'write', path: 'source/main.lua', content: check.input }] }); const ran = await editor.request('run'); await new Promise((resolve) => setTimeout(resolve, 300)); return ran.error === null;", 30);
    await send("Input.dispatchMouseEvent", { type: "mousePressed", x: 320, y: 180, button: "left", clickCount: 1 }, sessionId);
    await send("Input.dispatchMouseEvent", { type: "mouseReleased", x: 320, y: 180, button: "left", clickCount: 1 }, sessionId);
    await send("Input.dispatchKeyEvent", { type: "keyDown", code: "Space", key: " ", windowsVirtualKeyCode: 32 }, sessionId);
    await send("Input.dispatchKeyEvent", { type: "keyUp", code: "Space", key: " ", windowsVirtualKeyCode: 32 }, sessionId);
    await step("took input", "await new Promise((resolve) => setTimeout(resolve, 500)); const answer = await editor.request('console', { line: 'lua keys .. \\',\\' .. clicks .. \\',\\' .. math.floor(x) .. \\',\\' .. math.floor(y)' }); const [keys, clicks, x, y] = answer.output.trim().split(',').map(Number); return keys === 1 && clicks === 1 && Math.abs(x - 320 * devicePixelRatio) <= 2 && Math.abs(y - 180 * devicePixelRatio) <= 2;", 20);
    await step("read a gamepad", "Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [check.pad] }); await editor.request('edit', { operations: [{ op: 'write', path: 'source/main.lua', content: check.gamepad }] }); await editor.request('run'); await new Promise((resolve) => setTimeout(resolve, 500)); const answer = await editor.request('console', { line: 'lua table.concat(pad, \\',\\')' }); return answer.output.trim() === 'true,' + check.pad.id + ',true,1.00';", 20);
    await step("mirrored the interface", "await editor.request('edit', { operations: [{ op: 'write', path: 'source/main.lua', content: check.interface }] }); await editor.request('run'); let node = null; for (let tries = 0; tries < 50 && node === null; ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); node = document.querySelector('#haylen-accessibility [role=button][aria-label=Play]'); } node.click(); await new Promise((resolve) => setTimeout(resolve, 500)); const answer = await editor.request('console', { line: 'lua tostring(clicked)' }); return answer.output.includes('true');", 20);
    await step("answered through a plugin", "const context = Module.haylen.createPluginContext('probe', {}); context.register('echo', (params) => ({ echoed: params.text })); context.registerScreen('pick', (params, handle) => handle.resolve({ picked: params.choice })); const frames = context.videoStream('frames'); const tone = context.audioStream('tone', { sampleRate: 48000, channels: 1 }); const canvas = new OffscreenCanvas(8, 4); canvas.getContext('2d').fillRect(0, 0, 8, 4); let pushed = 0; let sounded = 0; const pusher = setInterval(() => { pushed += frames.push(canvas) ? 1 : 0; sounded += tone.push(new Float32Array(480)); }, 50); await editor.request('edit', { operations: [{ op: 'write', path: 'app.json', content: check.probeApp }, { op: 'write', path: 'plugins/probe/plugin.json', content: check.probePlugin }, { op: 'write', path: 'plugins/probe/source/init.lua', content: check.probeSource }, { op: 'write', path: 'source/main.lua', content: check.probeMain }] }); await editor.request('run'); let output = ''; for (let tries = 0; tries < 50 && !output.startsWith('hello,blue,true'); ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); output = (await editor.request('console', { line: \"lua (results.echo or 'none') .. ',' .. (results.screen or 'none') .. ',' .. tostring(stream.frameCount > 0) .. ',' .. stream.width\" })).output.trim(); } clearInterval(pusher); return output === 'hello,blue,true,8' && pushed > 0 && sounded > 0;", 20);
    await send("Runtime.evaluate", { expression: `check.clip = "${readFileSync(new URL("../data/video/clip.mp4", import.meta.url)).toString("base64")}"`, returnByValue: true }, sessionId);
    await step("played a video", "const clip = Uint8Array.from(atob(check.clip), (character) => character.charCodeAt(0)); await editor.request('edit', { operations: [{ op: 'write', path: 'app.json', content: check.app }, { op: 'remove', path: 'plugins/probe/plugin.json' }, { op: 'remove', path: 'plugins/probe/source/init.lua' }, { op: 'write', path: 'content/clip.mp4', content: clip }, { op: 'write', path: 'source/main.lua', content: check.videoMain }] }); await editor.request('run'); let output = ''; for (let tries = 0; tries < 80 && !output.startsWith('ready,true'); ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); output = (await editor.request('console', { line: \"lua clip.state .. ',' .. tostring(clip.width > 0 and clip.frameCount > 0)\" })).output.trim(); } return output === 'ready,true';", 20);
}
if (ready) {
    await step("ran", "await editor.request('clearFiles'); const edited = await editor.request('edit', { operations: [{ op: 'write', path: 'app.json', content: check.app }, { op: 'write', path: 'source/main.lua', content: check.main }, { op: 'write', path: 'source/settings.lua', content: check.first }] }); const ran = await editor.request('run'); return ran.revision === edited.revision && ran.app.name === 'Editor Check' && ran.error === null;", 30);
    await step("reloaded", "await editor.request('edit', { operations: [{ op: 'write', path: 'source/settings.lua', content: check.second }] }); const applied = await editor.request('applyChanges'); return applied.report.modules[0] === 'source/settings.lua' && applied.report.rejected.length === 0;", 20);
    await step("rejected a syntax error", "await editor.request('edit', { operations: [{ op: 'write', path: 'source/settings.lua', content: 'return {step = ' }] }); const applied = await editor.request('applyChanges'); const rejected = applied.report.rejected[0]; return rejected.path === 'source/settings.lua' && rejected.error.code === 'syntax' && rejected.error.phase === 'reload' && rejected.error.file === 'source/settings.lua' && rejected.error.line === 1;", 20);
    await step("restored", "await editor.request('edit', { operations: [{ op: 'write', path: 'source/settings.lua', content: check.second }] }); const applied = await editor.request('applyChanges'); const answer = await editor.request('console', { line: 'lua settings.step' }); return applied.report.restored[0] === 'source/settings.lua' && applied.report.resumed && answer.output.includes('2');", 20);
    await step("applied atomically", "const before = await editor.request('edit', { operations: [] }); let refused = null; try { await editor.request('edit', { operations: [{ op: 'write', path: 'source/extra.lua', content: 'return 1' }, { op: 'remove', path: 'source/missing.lua' }] }); } catch (error) { refused = error.code; } let invalid = null; try { await editor.request('edit', { operations: [{ op: 'write', path: '../outside.lua', content: '' }] }); } catch (error) { invalid = error.code; } const renamed = await editor.request('edit', { operations: [{ op: 'rename', from: 'source/settings.lua', to: 'source/moved.lua' }, { op: 'rename', from: 'source/moved.lua', to: 'source/settings.lua' }] }); return refused === 'missingFile' && invalid === 'invalidPath' && renamed.revision === before.revision + 1;", 20);
    await step("kept the project budget", "let large = null; try { await editor.request('edit', { operations: [{ op: 'write', path: 'content/large.bin', content: new Uint8Array(2 * 1048576) }] }); } catch (error) { large = error.code; } let many = null; try { await editor.request('edit', { operations: Array.from({ length: 20 }, (_, index) => ({ op: 'write', path: 'content/item-' + index + '.txt', content: 'item' })) }); } catch (error) { many = error.code; } let broken = null; try { await editor.request('loadZip', { bytes: new Uint8Array([1, 2, 3]) }); } catch (error) { broken = error.code; } return large === 'projectBudget' && many === 'projectBudget' && broken === 'package';", 20);
    await step("stopped endless code", "check.errors = []; await editor.request('edit', { operations: [{ op: 'write', path: 'source/main.lua', content: check.endless }] }); const started = performance.now(); await editor.request('run'); for (let tries = 0; tries < 150 && check.errors.length === 0; ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); } const error = check.errors[0]; return error !== undefined && error.code === 'executionBudget' && error.callback === 'update' && error.file === 'source/main.lua' && performance.now() - started < 10000;", 30);
    await step("inspected", "await editor.request('edit', { operations: [{ op: 'write', path: 'source/main.lua', content: check.main }] }); await editor.request('run'); const answer = await editor.request('inspect', { path: 'globals.settings' }); return answer.entries.some((entry) => entry.key === 'step');", 20);
    await step("ran again in place", "for (let round = 0; round < 4; ++round) { await editor.request('run'); } await new Promise((resolve) => setTimeout(resolve, 500)); return Module.haylen.audio.stats().available;", 30);
    await step("stopped early", "editor.request('run'); await editor.request('stop'); const ran = await editor.request('run'); await new Promise((resolve) => setTimeout(resolve, 500)); return ran.error === null;", 30);
    await step("disposed", "const answer = await editor.request('dispose'); let after = null; try { await editor.request('hello'); } catch (error) { after = error.code; } return answer.disposed && after === 'disposed';", 30);
}
if (ready && worker) {
    await navigate();
    await step("ready again", readyScript, 40);
    await step("terminated a stuck worker", "await editor.request('clearFiles'); await editor.request('edit', { operations: [{ op: 'write', path: 'app.json', content: check.app }, { op: 'write', path: 'source/main.lua', content: check.stuck }] }); editor.request('run').catch(() => undefined); for (let tries = 0; tries < 100 && Module.haylen.editor.heartbeat() < 2000; ++tries) { await new Promise((resolve) => setTimeout(resolve, 100)); } const silent = Module.haylen.editor.heartbeat() >= 2000; const waiting = editor.request('hello').then(() => null, (error) => error.code); await Module.haylen.editor.terminate(); let after = null; try { await editor.request('hello'); } catch (error) { after = error.code; } return silent && (await waiting) === 'terminated' && after === 'terminated';", 30);
}
// The errors that the page heard through "onError" join the log, since a runtime in a worker logs to a console of its own.
if (result.failure) {
    const heard = await send("Runtime.evaluate", { expression: "JSON.stringify(window.check ? check.errors : [])", returnByValue: true }, sessionId);
    result.log.push("Errors of the page: " + (heard.result?.value ?? "none"));
}
console.log(JSON.stringify(result));
process.exit(0);
