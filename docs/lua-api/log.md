# 📖 `haylen.log`

The module `haylen.log` writes messages to the engine log, each with a level that tells how serious it is. Lines from scripts, from the engine and from the `log` module of Varn go through one logger, so they appear together in one place on every platform. Use it for messages that help you follow what the app does. To stop the app and show the error screen instead, use [`haylen.reportError`](haylen.md#haylenreporterrormessage).

```lua
local log = require('haylen.log')
```

## Levels

The log has four levels, from the lowest to the highest: `'debug'`, `'info'`, `'warning'` and `'error'`. The current level is the lowest level that the log writes, and it drops every message below it. It starts at `'info'`, so `log.debug` writes nothing until the app lowers the level.

## Where the lines go

Every line carries its time and its level, such as `[2026-01-02 10:00:00.123] [info] Ready`. Each platform writes the lines to its usual place.

| Platform | Place |
| --- | --- |
| macOS, Windows and Linux | The standard output, with warnings and errors on the standard error. |
| iOS, iPadOS, tvOS and Mac Catalyst | The unified log of the system, with the bundle identifier of the app as the subsystem and `haylen` as the category. |
| Android | Logcat, with the tag `haylen`. |
| Web | The console of the browser, with warnings through `console.warn` and errors through `console.error`. |

The command [`haylen run`](../tools/distribution.md#run) shows these lines while the app runs, and [`debug.recentLog(count)`](debug.md#debugrecentlogcount) returns the newest ones inside the app.

## Functions

### log.debug(...)

Writes a message at the level `'debug'`. The function converts every argument with `tostring`, so `__tostring` metamethods apply, and joins the parts with tab characters, the way `print` joins them. Nothing is written while the current level is above `'debug'`. The arguments are converted even then, so check [`log.level()`](#loglevel) first when a message is costly to build.

```lua
local log = require('haylen.log')

local player = {x = 120, y = 48}
log.debug('The player is at', player.x, player.y)
```

### log.info(...)

Writes a message at the level `'info'`, with the arguments handled like [`log.debug`](#logdebug).

```lua
local log = require('haylen.log')

log.info('The level loaded:', 'forest', 3)
```

### log.warning(...)

Writes a message at the level `'warning'`, with the arguments handled like [`log.debug`](#logdebug).

```lua
local log = require('haylen.log')

local coins = -5
if coins < 0 then
    log.warning('The coins went negative:', coins)
end
```

### log.error(...)

Writes a message at the level `'error'`, with the arguments handled like [`log.debug`](#logdebug). It only writes. The app keeps running, and no Lua error is raised.

```lua
local log = require('haylen.log')

local ok, failure = pcall(function() error('The spawn point is missing.') end)
if not ok then
    log.error('The level could not start:', failure)
end
```

### log.level()

Returns the current level, one of `'debug'`, `'info'`, `'warning'` or `'error'`.

```lua
local log = require('haylen.log')

if log.level() == 'debug' then
    log.debug('Every enemy position:', table.concat({'10,20', '40,8', '72,64'}, ' '))
end
```

### log.setLevel(level)

Sets the current level, one of `'debug'`, `'info'`, `'warning'` or `'error'`. It applies to the lines of scripts, of the engine and of the `log` module of Varn alike. The level belongs to the process, so it stays the same when the app restarts. Any other value raises an error that names it, such as `bad argument #1 to 'setLevel' (unknown value 'loud')`.

```lua
local log = require('haylen.log')
local haylen = require('haylen')

-- Phones keep only the problems.
if haylen.platform == 'android' or haylen.platform == 'ios' then
    log.setLevel('warning')
end
log.debug('This line shows only on other platforms.')
log.warning('This line shows everywhere.')
```

## C++

C++ code writes to the same log through `haylen::core::Log`. Its methods `debug`, `info`, `warning` and `error` format the message with `std::format`, and `Log::setLevel` is the same level as `log.setLevel`. The method `Log::addListener` receives every line that the log writes, from any thread.

```cpp
#include <cstddef>

#include "haylen/core/Log.hpp"

class MapLoader final {
  public:
    void finish(std::size_t tiles) {
        haylen::core::Log::info("The map has {} tiles.", tiles);
    }
};
```

## Errors

| Message | Cause |
| --- | --- |
| `bad argument #1 to 'setLevel' (unknown value '<value>')` | The level of `log.setLevel` is not one of the four names. |
| `'__tostring' must return a string` | A `__tostring` metamethod of an argument returned something other than a string. |
