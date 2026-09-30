# Taskbar Quest

Taskbar Quest is a small game written in Lua on top of Haylen that lives on the desktop. A tiny hero walks along a transparent strip just above the taskbar, or the Dock on macOS, and fights the slimes and mushrooms that sprout ahead of it. Defeated enemies drop coins that fly to the gold counter, and the gold buys a sharper sword and healing potions. When the hero falls, it rests for a moment and gets up again.

The game shows the desktop windows of the engine. The strip has no title bar, lets the desktop show through its transparent pixels, stays above the other windows, never takes the keyboard from the app you are typing in, and lets every click through except over the hero, the enemies, the coins, the ground, the grip and the buttons, whose regions it gives the window every frame. The [desktop guide](../../../docs/desktop.md) explains these windows, and the [window reference](../../../docs/lua-api/window.md#desktop-windows) lists every function.

## What to try

- Click a slime or a mushroom to strike it, click a coin to collect it at once, and click the hero to say hello.
- Drag the strip by the ground or by the grip at its left end.
- Click the empty space above the ground: the click reaches the window or the desktop behind the strip.
- Press Window to turn the strip into a normal opaque window in the middle of the screen, and Strip to send it back above the taskbar, transparent again. Escape quits in window mode, and the cross quits in both modes.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run games/taskbar-quest` |
| macOS app | `python3 make.py run games/taskbar-quest --platform macos` |
| Windows app | `python3 make.py run games/taskbar-quest --platform windows` |
| Linux app | `python3 make.py run games/taskbar-quest --platform linux` |
| Browser | `python3 make.py run games/taskbar-quest --platform web` |

The command `python3 make.py package games/taskbar-quest -o taskbar-quest.zip` zips the package, which is `app.json`, `source/` and `content/` of this folder, and the desktop player runs the zip as well: `haylen taskbar-quest.zip`.

## Platforms

| Platform | Notes |
| --- | --- |
| macOS | Everything works, and transparency always does. The strip is unfocusable, so clicking it never activates the game, and the app you were typing in keeps the keyboard and the menu bar. |
| Windows | Everything works with the Direct3D 11 backend, which is the default. A build with the OpenGL backend opens an opaque window. |
| Linux | Transparency needs a compositing window manager. The strip hears the mouse only over its regions. |
| Web | The strip shows over the page, and the window options keep their values without effect. |
| iOS, iPadOS, tvOS, Android | Apps fill the screen there, so these devices are not the target of the game. |

## Package layout

The sample folder is the package. Only `app.json`, `source/` and `content/` ship, and this README stays behind.

```text
taskbar-quest/
  app.json               A frameless, transparent, always on top and unfocusable strip of 200 points anchored to the bottom of the work area and filling its width, with a design size of 1600 by 200 and expand scaling.
  source/
    main.lua             Loads the action map and the theme and opens the game.
    config.lua           Tuning values: pixel scale, hero, enemies, spawns, prices, window mode and draw layers.
    scenes/game.lua      The scene: input, update order, drawing and the regions that keep the mouse.
    systems/             The quest rules (quest.lua), the strip and window modes (desktop.lua), the ground and the sky (stage.lua), effects and art.
    entities/            The hero, the enemies and the coins.
    ui/hud.lua           The counters and the buttons.
  content/
    images/              The pixel art, drawn for this sample.
    ui/theme.json        A compact theme for the strip.
```
