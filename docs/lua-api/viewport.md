# haylen.viewport

`haylen.viewport` describes how the fixed design resolution from `app.json` maps onto the real framebuffer. Apps draw and lay out in design units, and the viewport scales them to any screen according to the scaling policy. Use this module to place the HUD inside the visible and safe areas and to convert between design units and framebuffer pixels. Positions in input events and in `haylen.input` are already in design units.

```lua
local viewport = require('haylen.viewport')
```

The design size and the scaling policy come from the `design` section of `app.json`, which defaults to 1920 by 1080 units with the `expand` policy, and [viewport.setScaling](#viewportsetscalingpolicy) and [viewport.setDesignSize](#viewportsetdesignsizewidth-height) change them while the app runs. The viewport is recomputed at the start of every frame, so after a resize or a rotation the new values are available from the next frame on.

## Scaling policies

| Name | Behavior |
| --- | --- |
| `'fit'` | Shows the whole design area and adds letterbox bars around it. |
| `'fill'` | Fills the screen and crops the design area. The visible rectangle is smaller than the design area. |
| `'stretch'` | Fills the screen with non-uniform scaling. |
| `'expand'` | Keeps the design area whole and centered and extends the visible area to fill the screen. The visible rectangle can start at negative coordinates and exceed the design size. |
| `'pixelPerfect'` | Like `fit`, but only with integer scale factors, so every design unit covers the same whole number of pixels. A framebuffer smaller than the design area shrinks it by the smallest integer divisor that fits, such as a half or a third, so the whole design area stays visible and every pixel covers the same whole number of design units. |

## Functions

### viewport.designSize()

Returns the design width and height in design units, as two numbers.

```lua
local viewport = require('haylen.viewport')

local width, height = viewport.designSize()
local center = {x = width / 2, y = height / 2}
print('design center', center.x, center.y)
```

### viewport.visibleRect()

Returns the region of design space that is visible on screen, as a `Rect` from `haylen.math`. With `expand` it grows beyond the design area on screens with a different aspect ratio, and with `fill` it is the cropped part of the design area.

```lua
local viewport = require('haylen.viewport')
local graphics2d = require('haylen.graphics2d')

require('haylen.scene').push({
    render = function(self)
        local visible = viewport.visibleRect()
        graphics2d.beginScreen()
        graphics2d.drawRect(visible, '#FF1B2B3A')
    end,
})
```

### viewport.safeRect()

Returns the visible design region that is not covered by notches, rounded corners or system bars, as a `Rect`. Place buttons and important HUD text inside it. The `windowSafeAreaChanged` event of [haylen.events](events.md#engine-events) announces every change with the new rectangle.

```lua
local viewport = require('haylen.viewport')
local graphics2d = require('haylen.graphics2d')

require('haylen.scene').push({
    renderUi = function(self)
        local safe = viewport.safeRect()
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Score 1200', safe:left() + 24, safe:top() + 24, {size = 40, color = '#FFFFFFFF'})
        graphics2d.drawText(nil, 'Pause', safe:right() - 24, safe:top() + 24, {size = 40, color = '#FFFFFFFF', anchor = {1, 0}})
    end,
})
```

### viewport.pixelRect()

Returns the framebuffer region, in pixels, that shows the visible design rectangle, as a `Rect`. It covers the whole framebuffer for `fill`, `stretch` and `expand`, and it leaves the letterbox bars outside for `fit` and `pixelPerfect`.

```lua
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local pixels = viewport.pixelRect()
local width, height = window.framebufferSize()
print('letterbox bars', pixels.x, width - pixels:right(), pixels.y, height - pixels:bottom())
```

### viewport.pixelsPerUnit()

Returns how many framebuffer pixels one design unit covers horizontally and vertically, as two numbers. Both values are equal for every policy except `stretch`.

```lua
local viewport = require('haylen.viewport')

local scaleX, scaleY = viewport.pixelsPerUnit()
-- A one pixel line on screen, expressed in design units.
local hairline = 1 / scaleX
print('hairline width', hairline, scaleY)
```

### viewport.toDesign(x, y)

Converts a framebuffer position in pixels to design units and returns the two coordinates.

```lua
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local width, height = window.framebufferSize()
local x, y = viewport.toDesign(width, height)
print('bottom right corner in design units', x, y)
```

### viewport.toFramebuffer(x, y)

Converts a design position to framebuffer pixels and returns the two coordinates.

```lua
local viewport = require('haylen.viewport')

local x, y = viewport.toFramebuffer(960, 540)
print('design center lands on pixel', x, y)
```

### viewport.scaling()

Returns the name of the active scaling policy, one of `'fit'`, `'fill'`, `'stretch'`, `'expand'` or `'pixelPerfect'`.

```lua
local viewport = require('haylen.viewport')

if viewport.scaling() == 'expand' then
    print('anchor the HUD to the visible rectangle')
end
```

### viewport.setScaling(policy)

Changes the [scaling policy](#scaling-policies) while the app runs, such as from a video settings screen. The viewport follows at once, so the functions of this module report the new mapping right away, and the UI, the default view of cameras and the canvases lay out and draw with it from the next frame on. The pointer and the fingers keep their places on the screen, so `input.mousePosition()` reports where the pointer lies under the new policy. A name that is not a policy raises `unknown value '<name>'`.

```lua
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

ui.mount(ui.segmentedControl{
    items = {{id = 'expand', text = 'Expand'}, {id = 'fit', text = 'Fit'}, {id = 'pixelPerfect', text = 'Pixel perfect'}},
    selected = viewport.scaling(),
    onChange = function(event)
        viewport.setScaling(event.value)
    end,
})
```

### viewport.setDesignSize(width, height)

Changes the design resolution while the app runs, in design units, and the viewport, the UI, cameras and pointer input follow it like they follow [viewport.setScaling](#viewportsetscalingpolicy). A width or height that is not positive raises `The design size needs a positive width and height.`.

```lua
local viewport = require('haylen.viewport')

-- A pixel art scene lays out on a small canvas that pixel perfect scaling enlarges.
viewport.setDesignSize(480, 270)
viewport.setScaling('pixelPerfect')
print(viewport.designSize())
```

### viewport.safeAreaSimulation()

Returns the simulated safe area as its device name or as its insets from the top clockwise, or `nil` while the device reports its own safe area.

```lua
local viewport = require('haylen.viewport')

print(viewport.safeAreaSimulation())
```

### viewport.setSafeAreaSimulation(value)

Replaces the safe area the device reports, to test a layout for other screens on a desktop. `value` is the name of a device, whose insets are scaled to the window in the orientation of the window, or insets in window points as one number, `{vertical, horizontal}` or `{top, right, bottom, left}`. `nil` goes back to the safe area of the device. The `debug.safeArea` option of `app.json` sets it at start, and [ui.setSafeAreaVisible](ui.md#uisetsafeareavisiblevisible) shows it.

| Device | Portrait | Landscape |
| --- | --- | --- |
| `'iphoneNotch'` | notch at the top, home indicator at the bottom | notch on the left and right, home indicator at the bottom |
| `'iphoneDynamicIsland'` | dynamic island at the top, home indicator at the bottom | dynamic island on the left and right, home indicator at the bottom |
| `'ipad'` | status bar at the top, home indicator at the bottom | the same |
| `'androidGestureBar'` | camera cutout at the top, gesture bar at the bottom | camera cutout on the left, gesture bar at the bottom |
| `'television'` | the title safe margins of a TV | the same |

An unknown device raises `There is no simulated device named <name>.`, and anything else that is not insets raises `A simulated safe area is a device name or one, two or four insets.`.

```lua
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

viewport.setSafeAreaSimulation('iphoneDynamicIsland')
ui.setSafeAreaVisible(true)
print(viewport.safeRect())

viewport.setSafeAreaSimulation({40, 0, 24, 0})
viewport.setSafeAreaSimulation(nil)
```
