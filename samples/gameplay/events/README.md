# Haylen Events

A Lua sample with one scene per feature of [haylen.signal](../../../docs/lua-api/signal.md), [haylen.events](../../../docs/lua-api/events.md) and the [lifecycle](../../../docs/lifecycle.md) of the engine. The menu lists the tests, each test opens as its own scene with a Back button, a log of what happened with the frame it happened on and the code it runs, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Signals | Listeners with a priority, a one-shot listener, a deferred listener, a blocked signal, a blocked connection and a listener that disconnects another one during the emit. |
| Owners | Guards and a banner document that own their listeners, which disconnect by themselves when a guard is collected or the banner is unmounted. |
| Event bus | Damage events on channels, a listener for every channel, a filter, a shield that consumes events with a high priority, and events posted for the end of the frame. |
| Scene scopes | An arena scene whose signal connection, event listener, timer, tween, task and card all end when it unloads. |
| Lifecycle log | Every engine event as it happens, with buttons for a scene that loads behind a loading view, a failed load, the pause, a simulated notch, object events, an asset, a document, a socket that retries and fullscreen. |
| Pause | A pause menu in the `whenPaused` mode, the paused and unpaused hooks of both scenes, and timers in the pausable and always modes. |
| Autoloads | The player data autoload that `app.json` lists, shared with a shop scene and drawing the coin counter on every screen, and a jukebox added at run time. |
| Classes | `haylen.class` with inheritance, `super`, a copied `__eq`, walking and flying mixins with their `included` hook and `is` checks. |
| Diagnostics | Live tables of `events.stats()` and `signal.list()`, the counts around the collection of an owner and the debug overlay. |

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/gameplay/events` |
| macOS app | `python3 make.py run samples/gameplay/events --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/gameplay/events --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/gameplay/events --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/gameplay/events --platform android --device <serial>` |
| Browser | `python3 make.py run samples/gameplay/events --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Use the panel of a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Open and close the pause menu | P | Start button | Buttons of the panel | Buttons of the panel |
| Close an overlay | Escape or its Close button | East button | Close button | Menu |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |

The lifecycle log also shows what the platform reports: switch to another window or tab, resize the window, plug in or remove a gamepad or turn a phone to see the app, window and gamepad events.
