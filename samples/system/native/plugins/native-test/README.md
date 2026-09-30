# Native Test

The local plugin of the [native sample](../../README.md) that stands in for the test library of the engine, `engine/tests/native/NativeTest.c`, in the browser. The browser loads no native libraries, so the web part of this plugin answers the functions of the library, its handlers and its events in the page, and the checks of the sample run against these answers. On every other platform the sample loads the library itself, which registers its handlers as `native_test.<name>`. The sample lists the plugin in its `app.json` with no parameters.

## Lua API

```lua
local nativeTest = require('native-test')
local platform = require('haylen.platform')

print(nativeTest.call('add', {a = 20, b = 22}):await())
local echo = platform.call(nativeTest.name('echo'), {word = 'hello'}):await()
```

| Function | Meaning |
| --- | --- |
| `nativeTest.name(name)` | The name of the handler or event `name` of the library on this platform: `native_test.<name>` where the library loads, and `native-test.<name>` in the browser. |
| `nativeTest.call(name, params)` | Calls the function `name` of the library in the browser, which the page answers with its result. |

| Method | Answer |
| --- | --- |
| `add`, `scale` | The sum of `a` and `b`, and the product of `value` and `factor`. |
| `origin` | `javascript`. |
| `pointAdd`, `rectGrow` | The sum of the points `a` and `b`, and `rect` grown by `amount` on every side. |
| `fill`, `checksum` | `size` bytes that count up from `seed`, and the FNV-1a hash of `bytes`. |
| `reportLater` | `null`, and a moment later the event `report` with `{value, data}`. |
| `init` | `true`, and a moment later the event `ready` with `{version, origin}`. |
| `echo` | `{echo, thread}` with the parameters, a moment later. |
| `fail` | A failure with the code `native_test_failure` and the data `{reason = 'requested'}`. |
| `wait` | Nothing until the app gives the call up, and then the event `cancelled` with `{call}`. |

## Platforms

Only the web has a part. The plugin needs no setup.
