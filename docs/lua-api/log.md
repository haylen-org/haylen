# haylen.log

The module `haylen.log` writes messages to the engine log. Lines from scripts and from the engine go through the same logger, so they appear together in the same console on every platform. Use it for diagnostics that should carry a severity. Use `haylen.reportError` from the root module when the app must stop and show an error screen.

```lua
local log = require('haylen.log')
```

## Functions

### log.debug(...)

Writes a message at the `debug` level. Every argument is converted with `tostring`, so `__tostring` metamethods apply, and the parts are joined with tab characters, the way `print` joins them. Nothing is written when the current level is above `debug`.

```lua
local log = require('haylen.log')

local player = {x = 120, y = 48}
log.debug('player at', player.x, player.y)
```

### log.info(...)

Writes a message at the `info` level, with the same argument handling as `log.debug`.

```lua
local log = require('haylen.log')

log.info('level loaded', 'forest', 3)
```

### log.warning(...)

Writes a message at the `warning` level, with the same argument handling as `log.debug`.

```lua
local log = require('haylen.log')

local coins = -5
if coins < 0 then
    log.warning('coins went negative:', coins)
end
```

### log.error(...)

Writes a message at the `error` level, with the same argument handling as `log.debug`. It only logs. The app keeps running and no Lua error is raised.

```lua
local log = require('haylen.log')

local ok, failure = pcall(function() error('missing spawn point') end)
if not ok then
    log.error('could not start the level:', failure)
end
```

### log.level()

Returns the lowest level that is written, one of `'debug'`, `'info'`, `'warning'` or `'error'`. The level starts at `'info'`, so `log.debug` writes nothing until the level is lowered.

```lua
local log = require('haylen.log')

if log.level() == 'debug' then
    log.debug('verbose diagnostics are on')
end
```

### log.setLevel(level)

Sets the lowest level that is written, for script and engine messages alike. Lines written through Varn's `log` module follow the same level. The argument `level` is one of `'debug'`, `'info'`, `'warning'` or `'error'`. Any other value raises `bad argument #1 to 'setLevel' (unknown value 'loud')`, naming the value.

```lua
local log = require('haylen.log')
local haylen = require('haylen')

-- Release builds on devices keep only problems.
if haylen.platform == 'android' or haylen.platform == 'ios' then
    log.setLevel('warning')
end
log.debug('hidden on phones')
log.warning('shown everywhere')
```
