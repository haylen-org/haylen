# 🔌 {{TITLE}}

A Haylen plugin that echoes messages through the native code of every platform. Its Lua API is the same on every platform, and `docs/platform/plugins.md` of the Haylen repository explains how plugins work.

## Installation

The command `plugin add` copies the plugin into `plugins/{{ID}}/` of the app:

```sh
haylen plugin add path/to/{{ID}} --app my-app
```

With `--reference`, the app keeps the plugin where it is, so apps that share the plugin follow its folder:

```sh
haylen plugin add path/to/{{ID}} --reference --app my-app
```

A plugin in a git repository is added from its address at a branch, tag or commit. With `--reference`, the tooling pins the commit in `haylen.lock` of the app, which a Release build needs, and `haylen plugin update {{ID}}` pins the commit that the ref names later:

```sh
haylen plugin add https://example.com/{{ID}}.git --ref 1.0.0 --reference --app my-app
```

Each of them lists the plugin in the `plugins` section of `haylen.json`. The values of its [parameters](#parameters) go under `parameters`, and a reference also names the folder of the plugin in `path`, relative to the app:

```json
{
    "plugins": {
        "{{ID}}": {"path": "../{{ID}}", "parameters": {}}
    }
}
```

## Platforms

| Platform | Native part | Setup |
| --- | --- | --- |
| iOS, iPadOS, Mac Catalyst, tvOS, visionOS, macOS | `apple/{{NAME}}Plugin.swift` | None. |
| Android | The library module in `android/` | None. |
| Web | `web/{{ID}}.js` | None. |
| Windows, Linux, and macOS in the desktop player | The C++ library of `native/` | None. The backend builds the library with CMake against the installed SDK of the engine and places it next to the app, and the Lua API loads it. |

## Parameters

The plugin has no parameters yet, so its entry in `haylen.json` is an empty object.

## Lua API

```lua
local plugin = require('{{ID}}')
```

### plugin.echo(message)

Sends `message` to the native part of the plugin, which answers with the same message. Returns a platform call of `haylen.platform`, whose `await` gives `{message = message}`, or `nil` and the error of the call.

```lua
local async = require('async')
local plugin = require('{{ID}}')

async.spawn(function()
    local result, err = plugin.echo('hello'):await()
    print(result and result.message or tostring(err))
end)
```

### plugin.onEchoed(listener)

Calls `listener({message = message})` after every echo and returns the connection of the listener, whose `disconnect()` stops it.

```lua
local plugin = require('{{ID}}')

local echoes = plugin.onEchoed(function(payload)
    print('Echoed ' .. payload.message)
end)
```

## Native API

| Method or event | Params or payload | Answer |
| --- | --- | --- |
| `{{ID}}.echo` | `{message}` | `{message}` |
| `{{ID}}.echoed` (event) | `{message}` | |
