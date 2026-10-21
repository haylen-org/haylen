# 🗡️ Taskbar Quest

Taskbar Quest is a small game written in Lua on top of Haylen that lives on the desktop. A tiny hero walks along a transparent strip just above the taskbar, or the Dock on macOS, and fights the slimes and mushrooms that sprout ahead of it. Defeated enemies drop coins that fly to the gold counter, and the gold buys a sharper sword and healing potions. When the hero falls, it rests for a moment and gets up again.

The game shows the desktop windows of the engine. The strip has no title bar, lets the desktop show through its transparent pixels, stays above the other windows, never takes the keyboard from the app you are typing in, and lets every click through except over the hero, the enemies, the coins, the ground, the grip and the buttons, whose regions it gives the window every frame. The [desktop guide](../../../docs/platform/desktop.md) explains these windows, and the [window reference](../../../docs/lua-api/window.md#desktop-windows) lists every function.

## What to try

- Click a slime or a mushroom to strike it, click a coin to collect it at once, and click the hero to say hello.
- Drag the strip by the ground or by the grip at its left end.
- Click the empty space above the ground: the click reaches the window or the desktop behind the strip.
- Press Window to turn the strip into a normal opaque window in the middle of the screen, and Strip to send it back above the taskbar, transparent again. Escape quits in window mode, and the cross quits in both modes.

## Running it

The commands run from the root of the repository:

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `haylen run samples/games/taskbar-quest` |
| macOS app | `haylen run samples/games/taskbar-quest --platform macos` |
| Windows app | `haylen run samples/games/taskbar-quest --platform windows` |
| Linux app | `haylen run samples/games/taskbar-quest --platform linux` |
| Browser | `haylen run samples/games/taskbar-quest --platform web` |

The command `haylen package samples/games/taskbar-quest -o taskbar-quest.zip` zips the package, which is `app.json`, `source/` and `content/` of this folder, and the desktop player runs the zip as well: `haylen taskbar-quest.zip`.

## Platforms

| Platform | Notes |
| --- | --- |
| macOS | Everything works, and transparency always does. The strip is unfocusable, so clicking it never activates the game, and the app you were typing in keeps the keyboard and the menu bar. |
| Windows | Everything works with the Direct3D 11 backend, which is the default. A build with the OpenGL backend opens an opaque window. |
| Linux | Transparency needs a compositing window manager. The strip hears the mouse only over its regions. |
| Web | The strip shows over the page, and the window options keep their values without effect. |
| iOS, iPadOS, tvOS, Android | Apps fill the screen there, so these devices are not the target of the game. |

## Art

Every image of the game is painted for it with the `codex` command line and its `$imagegen` skill in the style of [Tiny Island](../tiny-island/README.md#art), whose art is the standard of the repository, as the [project rules](../../../AGENTS.md#samples) describe, then cut, scaled and packed locally. Every prompt carries the key words of this guide. The atlas loads with the `linear` filter and the theme sets `imageFilter` to `linear`, because the art is painted in high definition and the screen scales it.

| Part | Rule |
| --- | --- |
| Look | Polished casual fantasy in high definition, chunky cute shapes and simple bold forms that read at the small size of the strip. The knight is super deformed, with a head about 40 percent of his height, seen from the side in a slight three-quarter view and facing right, like the slime and the mushroom. |
| Outline | A bold clean outline of even thickness in very dark navy `#1E1B33` around every character, item and icon, which the hills of the window mode take along their tops. |
| Shading | Soft cel shading with two tones per material and one soft highlight, light from the top left, clean flat areas without noise or texture. |
| Palette | The knight in royal blue `#2F86F6` and warm yellow `#FFC93C` under a steel helmet with a red plume, a green `#7BC74D` slime, a red and cream mushroom, gold coins, fresh grass `#7BC74D` over warm brown dirt, a coral red close icon and a slate `#2F3352` grip. |
| Sizes | The art counts in units of `config.pixel` design units and holds `config.detail` pixels in each, 4, so it draws at three quarters of its pixels: the knight in cells of 96 pixels, the slime and the mushroom in cells of 72 by 64, coins of 40, flowers and stones of 32, the ground tile of 192 by 48, the grip of 40 by 64, the cloud of 136 by 56, a round dot of 24 for the bursts and icons of 72. Every frame stands on the bottom of its cell with its body in the middle. |
| Atlas | `content/images/quest.png` with `quest.json`, whose tags are the animations, `hero idle`, `hero walk`, `hero attack` and `hero hurt`, `slime move` and `slime hurt`, `mushroom move` and `mushroom hurt`, `coin spin` and `decorations`, and whose frames `ground`, `grip`, `cloud` and `dot` name the single pieces. Each animation of an enemy repeats frames of its sheet in the order it plays them. |

## Package layout

The sample folder is the package. Only `app.json`, `source/` and `content/` ship, and this README stays behind.

```text
taskbar-quest/
  app.json               A frameless, transparent, always on top and unfocusable strip of 200 points anchored to the bottom of the work area and filling its width, with a design size of 1600 by 200 and expand scaling.
  source/
    main.lua             Loads the action map and the theme and opens the game.
    config.lua           Tuning values: the scale of the art, hero, enemies, spawns, prices, window mode and draw layers.
    scenes/game.lua      The scene: input, update order, drawing and the regions that keep the mouse.
    systems/             The quest rules (quest.lua), the strip and window modes (desktop.lua), the ground and the sky (stage.lua), effects and art.
    entities/            The hero, the enemies and the coins.
    ui/hud.lua           The counters and the buttons.
  content/
    images/              The atlas of the art with its frames and tags, and the icons of the interface.
    ui/theme.json        A compact theme for the strip.
```
