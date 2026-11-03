# 📖 `haylen.viewport`

The module `haylen.viewport` describes how the design area of the app maps onto the real framebuffer, the image the GPU draws for the window. Use it to place the HUD inside the visible and safe areas, and to convert between design units and framebuffer pixels. It also changes the design size and the scaling policy while the app runs. The [rendering guide](../graphics/rendering.md#design-units-and-the-viewport) explains design units and how canvases use them.

```lua
local viewport = require('haylen.viewport')
```

The engine works the mapping out again at the start of every frame, so after a resize or a rotation the functions report the new values from the next frame on. Positions in input events and in [`haylen.input`](input.md) already arrive in design units.

## Scaling policies

The scaling policy decides how the design area fits the framebuffer. The `design` section of `app.json` picks it at start, and `viewport.setScaling` changes it.

| Name | Behavior |
| --- | --- |
| `'fit'` | Shows the whole design area and adds letterbox bars around it. |
| `'fill'` | Fills the screen and crops the design area. The visible rectangle is smaller than the design area. |
| `'stretch'` | Fills the screen with a different scale on each axis. |
| `'expand'` | Keeps the design area whole and centered and extends the visible area to fill the screen. The visible rectangle can start at negative coordinates and pass the design size. |
| `'fitWidth'` | Fits the design width to the width of the screen, and extends or crops the visible area above and below the centered design area. So a layout that follows the width, such as a scrolling menu or a portrait app, always fills the screen from side to side. |
| `'none'` | Scales nothing: a design unit is a point of the screen, as [`window.dpiScale()`](window.md) counts pixels per point. The visible area starts at the top left of the design area and grows and shrinks with the window, which suits tools and apps whose layout follows the window. |
| `'pixelPerfect'` | Like `fit`, but only with whole scale factors, so every design unit covers the same whole number of pixels. A framebuffer smaller than the design area shrinks it by the smallest whole divisor that fits, such as a half or a third, so the whole design area stays visible and every pixel covers the same whole number of design units. |

```lua
local viewport = require('haylen.viewport')

if viewport.scaling() == 'expand' then
    print('Anchor the HUD to the visible rectangle.')
end
```

## Design size and scaling

### viewport.designSize()

Returns the width and height of the design area in design units, as two numbers. They are 1920 and 1080 unless the `design` section of `app.json` or `viewport.setDesignSize` gives others. An app whose `orientation` is `any` turns the design with the screen, so a design of 440 by 956 returns 956 and 440 while the screen is wider than tall, as the [rendering guide](../graphics/rendering.md#design-units-and-the-viewport) explains.

```lua
local viewport = require('haylen.viewport')

local width, height = viewport.designSize()
local center = {x = width / 2, y = height / 2}
print('Design center', center.x, center.y)
```

### viewport.setDesignSize(width, height)

Changes the design size while the app runs, in design units. The viewport, the UI, cameras and pointer input follow it the way they follow [`viewport.setScaling`](#viewportsetscalingpolicy). A width or height that is not positive raises `The design size needs a positive width and height.`.

```lua
local viewport = require('haylen.viewport')

-- A pixel art scene lays out on a small canvas that pixel perfect scaling enlarges.
viewport.setDesignSize(480, 270)
viewport.setScaling('pixelPerfect')
print(viewport.designSize())
```

### viewport.scaling()

Returns the name of the [scaling policy](#scaling-policies) in use: `'fit'`, `'fill'`, `'stretch'`, `'expand'`, `'pixelPerfect'`, `'fitWidth'` or `'none'`. It is `'expand'` unless the `design` section of `app.json` or `viewport.setScaling` picks another.

```lua
local viewport = require('haylen.viewport')

print('This app scales with ' .. viewport.scaling())
```

### viewport.setScaling(policy)

Changes the [scaling policy](#scaling-policies) while the app runs, such as from a video settings screen. The viewport follows at once, so the functions of this module report the new mapping right away. The UI, the default view of cameras and the canvases lay out and draw with it from the next frame on. The pointer and the fingers keep their places on the screen, so `input.mousePosition()` reports where the pointer lies under the new policy. A name that is not a policy raises a `bad argument` error that contains `unknown value '<name>'`.

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

## Areas of the screen

### viewport.visibleRect()

Returns the region of design space that the screen shows, as a `Rect` from `haylen.math`. With `expand` it grows past the design area on screens of another shape, and with `fill` it is the cropped part of the design area. On a screen of the shape of the design area, every policy shows exactly the design area, from 0, 0. Screen canvases draw in this space.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

scene.push({
    render = function(self)
        local visible = viewport.visibleRect()
        graphics2d.beginScreen()
        graphics2d.drawRect(visible, '#FF1B2B3A')
    end,
})
```

### viewport.safeRect()

Returns, as a `Rect`, the visible design region that no notch, rounded corner, system bar or native view of a plugin covers, such as a banner ad. Each edge of the safe area of the device grows to the largest reservation on it, which [`viewport.reservedInsets`](#viewportreservedinsets) reports. Place buttons and important HUD text inside it, and UI anchored to the safe area follows it on its own. The event `windowSafeAreaChanged` of [`haylen.events`](events.md#engine-events) announces every change with the new rectangle.

Where nothing covers the window, the safe area is the whole visible region. Each platform reports only what lies over the app:

| Platform | What it reports |
| --- | --- |
| macOS | The camera housing of a notched Mac, for a fullscreen or borderless window that reaches it. |
| iOS, iPadOS and Mac Catalyst | The insets of the window. On iPadOS the top inset also clears the controls of a window that the system draws in its top corner, so nothing anchored to the safe area sits under them. |
| Android | The cutout and the system bars that stay visible. |
| Web | The part of the CSS `env(safe-area-inset-*)` insets that reaches into the canvas. |
| Windows and Linux | Nothing. |

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

scene.push({
    renderUi = function(self)
        local safe = viewport.safeRect()
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Score 1200', safe:left() + 24, safe:top() + 24, {size = 40, color = '#FFFFFFFF'})
        graphics2d.drawText(nil, 'Pause', safe:right() - 24, safe:top() + 24, {size = 40, color = '#FFFFFFFF', anchor = {1, 0}})
    end,
})
```

### viewport.reservedInsets()

Returns the edges of the screen that native views of plugins reserve, as a table with `left`, `top`, `right` and `bottom` in design units, the largest reservation on each edge. A plugin that shows a native view reserves the edge the view sits on, such as the bottom for a banner, and releases it when the view goes away. The engine reads the reservations at the start of every frame, and [`viewport.safeRect`](#viewportsaferect) already leaves them out.

```lua
local events = require('haylen.events')
local viewport = require('haylen.viewport')

events.on('windowSafeAreaChanged', function()
    local reserved = viewport.reservedInsets()
    if reserved.bottom > 0 then
        print('A native view takes the bottom ' .. reserved.bottom .. ' units of the screen.')
    end
end)
```

### viewport.cutouts()

Returns the cutouts of the screen over the window, as a list of `Rect` in design units. A cutout is a part of the screen that shows no pixels of the app, such as the notch or the dynamic island of an iPhone, the hole of a camera on Android or the housing of the camera of a notched Mac. The safe area already leaves the edge of each cutout out, so the cutouts serve an app that draws around them, such as a HUD that fills the room on both sides of a dynamic island. The list is empty where nothing cuts into the window. The event `windowCutoutsChanged` of [`haylen.events`](events.md#engine-events) announces every change, such as after a rotation.

| Platform | Cutouts |
| --- | --- |
| iOS and iPadOS | The regions that the screen occludes, which UIKit reports from iOS 27.1, such as the dynamic island and the camera of the foldable iPhone, in an engine built with the SDK of iOS 27.1 or later. Earlier releases and engines built with an older SDK report none. |
| Android | The bounding rectangles of the cutouts of the display. |
| macOS | The housing of the camera of a notched display, while the window reaches the top of that screen, such as in fullscreen. |
| Web | None, since browsers report the insets that cutouts take through `env(safe-area-inset-*)` but never their shapes. |
| Windows, Linux, tvOS, visionOS, Mac Catalyst | None, since these platforms report no cutouts. |

A [safe area simulation](#safe-area-simulation) of a device brings the cutout of its screen too.

```lua
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local viewport = require('haylen.viewport')

local cutouts = viewport.cutouts()
events.on('windowCutoutsChanged', function()
    cutouts = viewport.cutouts()
end)

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        for _, cutout in ipairs(cutouts) do
            graphics2d.drawRectOutline({cutout.x, cutout.y, cutout.width, cutout.height}, 2, '#FFF2B23A')
        end
    end,
})
```

### viewport.pixelRect()

Returns the framebuffer region, in pixels, that shows the visible design rectangle, as a `Rect`. It covers the whole framebuffer for `fill`, `stretch`, `expand`, `fitWidth` and `none`, and it leaves the letterbox bars outside for `fit` and `pixelPerfect`. Its edges lie on whole pixels, where the GPU places the viewport. So a centered area never starts half a pixel in, and `viewport.toDesign` maps every point to what the screen shows there.

```lua
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local pixels = viewport.pixelRect()
local width, height = window.framebufferSize()
print('Letterbox bars', pixels.x, width - pixels:right(), pixels.y, height - pixels:bottom())
```

## Conversions

### viewport.pixelsPerUnit()

Returns how many framebuffer pixels one design unit covers across and down, as two numbers. Both values are equal for every policy except `stretch`.

```lua
local viewport = require('haylen.viewport')

local scaleX, scaleY = viewport.pixelsPerUnit()
-- A line one pixel wide on screen, in design units.
local hairline = 1 / scaleX
print('Hairline width', hairline, scaleY)
```

### viewport.toDesign(x, y)

Converts a framebuffer position in pixels to design units and returns the two coordinates.

```lua
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local width, height = window.framebufferSize()
local x, y = viewport.toDesign(width, height)
print('Bottom right corner in design units', x, y)
```

### viewport.toFramebuffer(x, y)

Converts a design position to framebuffer pixels and returns the two coordinates.

```lua
local viewport = require('haylen.viewport')

local x, y = viewport.toFramebuffer(960, 540)
print('The design center lands on the pixel', x, y)
```

## Safe area simulation

### viewport.safeAreaSimulation()

Returns the simulated safe area, as its device name or as its insets from the top clockwise, or `nil` while the device reports its own safe area.

```lua
local viewport = require('haylen.viewport')

print(viewport.safeAreaSimulation())
```

### viewport.setSafeAreaSimulation(value)

Replaces the safe area that the device reports, to test a layout for other screens on a desktop. The argument `value` is one of these:

- The name of a device from the table below, whose insets are scaled to the window in the orientation of the window.
- Insets in points of the window: one number for every side, `{vertical, horizontal}` or `{top, right, bottom, left}`.
- The value `nil`, which goes back to the safe area of the device.

The option `debug.safeArea` of `app.json` sets it at start, and [`ui.setSafeAreaVisible`](ui.md#uisetsafeareavisiblevisible) shows it.

| Device | Portrait | Landscape | Cutout |
| --- | --- | --- | --- |
| `'iphoneNotch'` | Notch at the top, home indicator at the bottom | Notch on the left and right, home indicator at the bottom  The notch at the top center in portrait, along the left edge in landscape |
| `'iphoneDynamicIsland'` | Dynamic island at the top, home indicator at the bottom | Dynamic island on the left and right, home indicator at the bottom | The dynamic island below the top center in portrait, along the left edge in landscape |
| `'ipad'` | Status bar at the top, home indicator at the bottom | The same | None |
| `'androidGestureBar'` | Camera cutout at the top, gesture bar at the bottom | Camera cutout on the left, gesture bar at the bottom | The hole of the camera at the top center in portrait, along the left edge in landscape |
| `'television'` | The title safe margins of a TV | The same | None |

An unknown device raises `There is no simulated device named "<name>".`, and a negative inset raises `A simulated safe area needs non-negative insets.`. Anything else that is not insets raises `A simulated safe area is a device name or one, two or four insets.`.

```lua
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

viewport.setSafeAreaSimulation('iphoneDynamicIsland')
ui.setSafeAreaVisible(true)
print(viewport.safeRect())

viewport.setSafeAreaSimulation({40, 0, 24, 0})
viewport.setSafeAreaSimulation(nil)
```

## Errors

| Message | Cause |
| --- | --- |
| `The design size needs a positive width and height.` | The size of `viewport.setDesignSize` is not positive. |
| `bad argument #1 to 'setScaling' (unknown value '<name>')` | The policy of `viewport.setScaling` has another name. |
| `There is no simulated device named "<name>".` | The device of `viewport.setSafeAreaSimulation` is unknown. |
| `A simulated safe area needs non-negative insets.` | An inset of `viewport.setSafeAreaSimulation` is negative or not a number. |
| `A simulated safe area is a device name or one, two or four insets.` | The value of `viewport.setSafeAreaSimulation` has another form. |
