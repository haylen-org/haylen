# {{TITLE}}

A Haylen plugin that echoes messages through the native code of every platform. Its Lua API is the same on every platform, and `docs/plugins.md` of the Haylen repository explains how plugins work.

## Installation

```sh
python3 haylen.py plugin add path/to/{{ID}} --app my-game
```

The command `plugin add` copies the plugin into `plugins/{{ID}}/` of the app and lists it in the `plugins` section of its `app.json`:

```json
{
    "plugins": {
        "{{ID}}": {}
    }
}
```

## Platforms

| Platform | Native part | Setup |
| --- | --- | --- |
| iOS, iPadOS, Mac Catalyst, tvOS, macOS | `apple/{{NAME}}Plugin.swift` | None. |
| Android | The library module in `android/` | None. |
| Web | `web/{{ID}}.js` | None. |
| Windows, Linux | None | Calls fail with the code `noHandler`. |

## Parameters

The plugin has no parameters, so its entry in `app.json` is an empty object.

## Lua API

```lua
local echo = require('{{ID}}')
```

### echo.echo(message)

Sends `message` to the native part of the plugin, which answers with the same message. Returns a platform call of `haylen.platform`, whose `await` gives `{message = message}`, or `nil` and the error of the call.

```lua
local async = require('async')
local echo = require('{{ID}}')

async.spawn(function()
    local result, err = echo.echo('hello'):await()
    print(result and result.message or tostring(err))
end)
```

### echo.onEchoed(listener)

Calls `listener({message = message})` after every echo and returns the connection of the listener, whose `disconnect()` stops it.

```lua
local echo = require('{{ID}}')

local echoes = echo.onEchoed(function(payload)
    print('echoed ' .. payload.message)
end)
```

## Native API

| Method or event | Params or payload | Answer |
| --- | --- | --- |
| `{{ID}}.echo` | `{message}` | `{message}` |
| `{{ID}}.echoed` (event) | `{message}` | |
