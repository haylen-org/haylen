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

Both list the plugin in the `plugins` section of `app.json`. The values of its [parameters](#parameters) go under `parameters`, and a reference also names the folder of the plugin in `path`, relative to the app:

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
| iOS, iPadOS, Mac Catalyst, tvOS, macOS | `apple/{{NAME}}Plugin.swift` | None. |
| Android | The library module in `android/` | None. |
| Web | `web/{{ID}}.js` | None. |
| Windows, Linux, and macOS in the desktop player | The C library of `native/{{NAME}}.c` | None. The tool `haylen.py` builds it with CMake and places it next to the app, and the Lua API loads it. |

## Parameters

{{PARAMETERS}}

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
{{BLOCK:README_API}}

## Native API

| Method or event | Params or payload | Answer |
| --- | --- | --- |
| `{{ID}}.echo` | `{message}` | `{message}` |
| `{{ID}}.echoed` (event) | `{message}` | |
{{BLOCK:README_NATIVE}}
