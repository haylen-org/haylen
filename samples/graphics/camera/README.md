# Haylen Camera

Haylen Camera is a Lua sample of the 2D camera of `haylen.graphics2d`: following, smoothing, limits, zoom, framing, shake, rotation, viewports, blending, parallax, pixel snap, picking and debug drawing. A menu lists one test per feature, each test is a scene with a Back button, and Escape, the B button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu.

| Test | What it shows |
| --- | --- |
| Dead zone and drag margins | A camera locked on the player, with a dead zone or with drag margins, drawn by `camera:drawDebug`. |
| Smoothing and look-ahead | Position smoothing with its speed and a look-ahead by the velocity of the player. |
| Limits | Limits wider and narrower than the view, with and without limit smoothing. |
| Zoom | The method `camera:zoomAt` from the wheel and pinches toward the pointer, and from keys, shoulders and triggers toward the middle, within zoom limits. |
| Framing several targets | The method `camera:frame` keeping two players and a wanderer in view. |
| Shake | Trauma shake, directional recoil, frequency, largest offset and decay. |
| Rotation | A turning view with rotation smoothing and `ignoreRotation`. |
| Split screen | Two players with a camera each, side by side or stacked. |
| Minimap | A zoomed-out camera in a corner viewport whose visibility mask leaves the details out. |
| Blending cameras | The function `graphics2d.blendCameras` tweened between the player and a landmark. |
| Parallax layers | Clouds, mountains, hills and grass with their own scroll scales, repetition and autoscroll. |
| Pixel snap | The same pixel art drifting with and without `pixelSnap`. |
| Screen to world | Hovering and selecting objects through `camera:screenToWorld` in a zoomed and rotated view, labeled with `camera:worldToScreen`. |
| Debug drawing | The view, the limits and the drag box of the main camera drawn from an overview in the corner. |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch |
| --- | --- | --- | --- |
| Walk | WASD or holding the left button | Left stick | Holding a finger |
| Second player | IJKL | Right stick | |
| Zoom | Wheel, Z and X, minus and plus | Shoulders and triggers | Pinch |
| Turn | Q and E | Right stick | |
| Select or shake | Click or F | X button | Tap |

Every setting of a test is a control in the panel on the right, which the mouse, touch, the arrow keys, the directional pad and a TV remote reach.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/graphics/camera` |
| macOS app | `python3 haylen.py run samples/graphics/camera --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/graphics/camera --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/graphics/camera --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/graphics/camera --platform android` |
| Browser | `python3 haylen.py run samples/graphics/camera --platform web` |

## Package layout

```text
camera/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Loads the action map and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The base scene of every test with its header and Back button, and the cursor.
    world.lua            The meadow the cameras look at, with visibility bits for terrain and details.
    player.lua           A walker steered by an action or by a held pointer.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    input/actions.json   The action map.
    images/              Mountains, hills, clouds, grass, a knight and tiles generated for the sample.
```
