# 🐞 Debugger

This guide explains the Lua debugger of Haylen: how an editor attaches to an app that runs in development, what breakpoints, steps and the uncaught errors filter do, how the debugger shows values and evaluates code, what happens on the device while the app halts, and what it costs. The reference of `debug.breakpoint` is in [`haylen.debug`](../lua-api/debug.md#debugbreakpoint).

## How it works

The engine runs the debugger itself, inside the development session of the app. An editor talks to it with the Debug Adapter Protocol, the protocol that most editors use for their debuggers. The editor attaches to a TCP port of your machine, and the development server of `haylen.py` carries every message between that port and the app over the connection that also pushes your saved files.

```text
editor ── TCP port 4711 ── haylen.py run ── development connection ── app
```

The same path reaches every platform: the desktop player, the iOS, tvOS and visionOS simulators, Mac Catalyst, the Android emulator through the `adb reverse` that the development server already sets up, iPhones, iPads, Apple TVs and Android devices on your network, and the web page. So you never forward a port yourself.

Only an app in development has a debugger. That is an app that `haylen.py run` starts in the `Debug` configuration, which passes `--dev-server`, or a web page that `run --platform web` serves. An app built from the templates without `haylen.py run`, a shipped app and a protected release never run in development, so their Lua has no hooks and no port listens for an editor.

## Attach an editor

Start the app with `haylen.py run`. The summary of the run names the port that the debugger listens on:

```text
Debugger        Attach an editor to port 4711 of this machine
```

The option `--debugger-port` picks another port, such as when two apps run at once:

```bash
./haylen run samples/games/tiny-island --platform ios --debugger-port 4712
```

Then attach the debugger of your editor to that port. Every client of the Debug Adapter Protocol needs the same three things: a TCP connection to `127.0.0.1` and the port, the request `attach` and no program to launch, since `haylen.py run` already started the app. Use any name for the type of the adapter, since the app answers whatever the editor calls it.

In Neovim with its common Debug Adapter Protocol plugin, the adapter is a server on the port:

```lua
local dap = require('dap')
dap.adapters.haylen = {type = 'server', host = '127.0.0.1', port = 4711}
dap.configurations.lua = {
    {type = 'haylen', request = 'attach', name = 'Attach to the Haylen app'},
}
```

In Emacs with a client of the protocol that connects to a port, the configuration names the host, the port and the request:

```elisp
(add-to-list 'dape-configs
             '(haylen modes (lua-mode lua-ts-mode)
                      host "127.0.0.1" port 4711
                      :request "attach" :type "haylen"))
```

Visual Studio Code attaches through a debug type that an extension registers. Any such type works when the configuration adds `debugServer`, which makes the editor talk to the port instead of starting an adapter of its own:

```json
{
    "name": "Attach to the Haylen app",
    "type": "lua",
    "request": "attach",
    "debugServer": 4711
}
```

The development server takes one editor at a time and turns away another one while an editor is attached. It keeps the breakpoints and the error filter that the editor set and gives them again to every app that connects later, such as an app that you relaunched. Inside a running app, they also outlive every hot reload and every restart of the app, because the debugger belongs to the development session of the process, not to the app.

## Breakpoints

A breakpoint halts the app before its line runs. Lines in the files of `source/` and in the Lua modules of plugins take breakpoints, and the editor shows the file and the line where the app halted.

Every breakpoint can also have:

- A condition, a Lua expression that runs in the frame of the line, so it reads its locals. The app halts only when the condition is true. A condition that fails halts the app and writes its error to the console of the editor, so you see why.
- A hit count, which counts the times the line runs while its condition holds. A number alone, such as `5`, halts at that hit, and `>= 5`, `> 5`, `== 5` and `% 5` halt from the fifth hit on, after it, at it and at every fifth hit.
- A log message, which turns the breakpoint into a log point. A log point writes its message to the console of the editor and never halts. Each Lua expression between braces turns into its value, such as `Hit {target.name} for {damage}`.

## Steps and pauses

While the app halts, the editor lets it go on or steps:

| Command | What the app does |
| --- | --- |
| Continue | It runs until a breakpoint, a pause or an error halts it again. |
| Step over | It halts at the next line of the same coroutine, at the same depth of calls or above. |
| Step in | It halts at the next line of the same coroutine, also inside a function that the line calls or a coroutine that it resumes. |
| Step out | It halts at the next line of the same coroutine after the current function returns. |
| Pause | It halts at the next line of Lua that runs, in any coroutine. |

Steps follow the coroutine that halted, also across the `await` of a task of the async library. A step over `async.sleep(1):await()` lets every other coroutine and every frame run while the task waits, and halts at the line after the `await` in the frame where the task goes on. A step that leaves the last function of a coroutine halts at the next line of Lua that runs anywhere.

## Uncaught errors

The filter `Uncaught errors` of the editor, which is on by default, halts the app where an error rises that no `pcall` or `xpcall` of the app catches, before its stack unwinds. So the stack, the locals and the upvalues are still there to look at. That covers the errors of every callback of the engine, such as the hooks of a scene, a timer or an event listener, and the errors of the tasks of the async library, also when code that awaits the task handles its failure later. Once you continue, the error goes on to the error screen as usual, and a step after an error continues too, because the stack of the error unwinds.

## Values and evaluations

Every frame of the stack has three scopes: `Locals`, with the arguments and locals of the function and its extra arguments as `(vararg 1)` and on, `Upvalues`, and `Globals`, which are the globals that the function sees. A table opens into its keys and values, sorted with the integer keys first, and its metatable as `(metatable)`. An object of a bound type, such as a sprite, a camera or a body, opens into the properties that its type declares, which the getters of the engine read, then its metatable and its user values. A function opens into its upvalues. A value whose metatable has the field `__name` shows that name, such as `Sprite: 0x6000...`.

Looking never runs code of the app. The debugger reads every value raw, so no `__index`, `__pairs`, `__len` or `__tostring` metamethod runs, the properties of bound objects come from the native getters of the engine, and the garbage collector holds while the app halts, so no finalizer runs either.

The console, the watches and the hovers of the editor evaluate Lua. An evaluation in a frame sees its locals first, then its upvalues, then its globals, and an assignment to a local of the frame stays in the evaluation, while an assignment to any other name sets a global. An evaluation while the app runs sees the globals. Every evaluation runs on a coroutine of its own and stops with an error after one second, so a loop that never ends never holds the debugger, and an evaluation that awaits fails, since nothing resumes it while the app halts. Evaluations, conditions and log messages are the only code that the debugger runs, and only because you wrote them.

## From the code of the app

The function `debug.breakpoint()` of `haylen.debug` halts the app at the line that calls it while an editor is attached, and does nothing otherwise, also in an app that ships. It suits a condition that is easier to write in the code than in the editor:

```lua
local debugging = require('haylen.debug')

if enemy.health < 0 then
    debugging.breakpoint()
end
```

## While the app halts

A halt holds the frame thread inside the code of the app until the editor lets it go on, and nothing else of the app runs: no frame, no timer, no event and no callback. The platform stays responsive, so the system never marks the app as hung:

- On the desktops, iOS, iPadOS, tvOS, visionOS and Mac Catalyst, the runtime keeps reading the events of the window and the system. Input, window changes and the moves to the background reach the app in order once it goes on. A quit, from the close button, the menu, Command+Q or the system, lets the app finish its frame without halting, and then the app quits as usual.
- On Android, the activity keeps taking its lifecycle and input on its own thread. When the system ends the window or the activity, which waits for the frame thread, the app finishes its frame without halting and lets the window go.
- Sounds that already play go on, because the mixer plays them on an audio thread of its own, and nothing new starts until the app goes on.

The editor shows the console line `The app goes on to the end of its frame without halting, since the platform asked it to finish.` when the platform ended a halt.

## The web

A web page cannot halt, because the browser runs the app on the one thread that also delivers every message, including the ones of the editor, and that thread delivers nothing until the frame returns. So on the web the debugger keeps the log points, which never halt, and turns down every other breakpoint, a pause and `debug.breakpoint()` with that reason, and it offers no filter of uncaught errors. Errors still reach the error screen and the page.

## Cost

An app without breakpoints, log points, steps or the error filter runs without any hook, exactly as fast as without an editor. While any of them is set, every coroutine reports its calls and returns, and only the functions whose lines hold a breakpoint report their lines, so code far from the breakpoints runs close to full speed. The breakpoints are indexed by file and line, so the check of a line costs one lookup. Turning the error filter off in the editor leaves an app with no breakpoints without hooks again.

## Release builds

The debugger exists only in the development session, which only `--dev`, `--dev-server` and the page of `run --platform web` create. A release build carries the bootstrap of its app, which ignores those options, as [Distribution](distribution.md#development-mode) explains, so no release ever hooks its Lua, and `haylen.py check --config Release` and the inspection of `run --config Release` keep proving that no release holds Lua text or files of the package as they are.
