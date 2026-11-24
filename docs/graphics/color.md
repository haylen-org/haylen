# 🌈 Color

This guide explains how the engine holds colors: the encoded colors that people pick, the linear light that lighting adds up, HDR colors brighter than white, the temperature of light, the color spaces of screens and the physical units of light. The [reference of `haylen.math`](../lua-api/math.md#color) lists every function of `Color`, `m.colorSpace` and `m.photometry`.

## Encoded colors and linear light

Screens and images do not store light as it is. They store it through the curve of sRGB, which spends more of the numbers on dark tones, where the eye tells small steps apart, and fewer on bright tones. So the value `0.5` of an image is not half the light of white: it is about a fifth of it. A `Color` holds this encoded form, the one that color pickers, themes and image files give.

Light itself adds up in linear form, where twice the number is twice the light. Mixing two colors, blurring, fading and every kind of lighting belong in linear light, and mixing encoded values gives colors that are too dark between them. Decode a color with `color:linear()` before such work and encode the result again with `m.fromLinear`.

```lua
local m = require('haylen.math')

local red, green = m.color('#FFFF0000'), m.color('#FF00FF00')
local encodedMix = red:lerp(green, 0.5)
local lightMix = m.fromLinear(red:linear():lerp(green:linear(), 0.5))
print(encodedMix, lightMix) -- #FF808000 #FFBCBC00
```

A color is decoded once and encoded once. Decoding a color that is already linear, or encoding one twice, applies the curve twice and makes everything too dark or too pale.

## HDR colors

A channel above `1` is light brighter than the white of the screen, such as the sun, a flame or a glowing sign. Such a color is an HDR color, for high dynamic range. Its channels keep their values through `color:linear()` and `m.fromLinear`, its luminance goes above `1`, and a number times a color scales its light while its alpha stays. A screen shows at most its white, so the end of the frame fits the light of the scene into what the screen shows.

```lua
local m = require('haylen.math')

local flame = m.color('#FFFF9A3C'):linear() * 6
print(flame:luminance() > 1) -- true
```

## Color temperature

The light of a hot body, such as the filament of a bulb or the sun, has a color that its temperature sets, measured in kelvin. Low temperatures glow red and orange, around 6500 kelvin the light is close to white, and higher temperatures turn blue. The function `m.fromTemperature` gives that color, which suits lights that should look like real lamps: about 1900 for a candle, 2700 for a warm bulb, 4000 for an office and 10000 for the shade under a clear sky.

```lua
local m = require('haylen.math')

local candle = m.fromTemperature(1900)
local office = m.fromTemperature(4000)
print(candle, office)
```

## Screens and their color spaces

A color space names the reddest red, the greenest green and the bluest blue that a screen shows, its primaries, and the curve of its signal. Most screens show sRGB. Many phones, tablets and Macs show Display P3, whose red and green go further, and HDR screens take the signal of HDR10, with the even wider primaries of Rec. 2020 and the perceptual quantizer, a curve that carries light up to 10000 nits.

A scene draws its light with the primaries of sRGB, and `m.colorSpace.encode` turns that light into the signal of the screen. A color that a space cannot show has channels below `0` or above `1` there, which `m.colorSpace.convert` shows when it moves a color from one space to another. On an HDR screen the white of the scene reaches the reference white, 203 nits by default, so interfaces and 2D art keep the brightness they have on other screens while highlights shine above it.

```lua
local m = require('haylen.math')

local highlight = m.color(1, 1, 1):linear() * 4
local signal = m.colorSpace.encode(highlight, 'hdr10')
print(string.format('%.0f nits', m.colorSpace.decodePq(signal.g))) -- 812 nits
```

## Physical light

Real light has units, and lights that use them keep the balance of the real world, where the sun is many thousands of times brighter than a lamp. Four units measure it:

- Lumens count all the light that a lamp gives, the number printed on the box of a bulb.
- Candelas measure the light that a lamp sends in one direction, which `m.photometry.pointIntensity` and `m.photometry.spotIntensity` find from lumens, since a spot sends the same lumens into less room.
- Lux measure the light that falls on a surface, about 100000 in direct sun and 500 in an office, which `m.photometry.illuminance` finds from candelas and the distance.
- Nits, candelas per square meter, measure the light that leaves a surface, such as a screen, a neon sign or the sky.

A camera fits that range into the screen with its exposure. The exposure value of `m.photometry.ev100FromCamera` follows the f-number, the shutter time and the ISO of a real camera, and `m.photometry.ev100FromLuminance` picks the exposure that shows a scene of a given average brightness as middle gray. The factor of `m.photometry.exposure` multiplies light in nits into the values of the screen, so an emissive surface of 1500 nits glows in a dark room and looks dim under the sun.

```lua
local m = require('haylen.math')

local bulb = m.photometry.pointIntensity(800)
local onDesk = m.photometry.illuminance(bulb, 1.5)
local exposure = m.photometry.exposure(m.photometry.ev100FromLuminance(50))
print(string.format('%.0f lux, %.3f', onDesk, 1500 * exposure))
```
