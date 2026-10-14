# Lighting

The module `haylen.lighting2d` lights 2D scenes: point, spot and directional lights, the occluders that cast their shadows, and the questions gameplay asks about light, such as whether a guard can see the player in the dark. This guide explains how a lit scene is set up, how lights choose what they reach, how shadows come from physics bodies and maps, and what lighting costs. The [`haylen.lighting2d` reference](../lua-api/lighting2d.md) lists the complete API, and the [rendering guide](../graphics/rendering.md#lighting) describes the passes on the GPU.

## A lit scene

A world or render target canvas of [`haylen.graphics2d`](../lua-api/graphics2d.md) becomes lit when it receives `ambientLight`. Its draws then also record their emission, the normal and specular strength of their surface and the light mask and layer that lights test, every light drawn into the canvas adds to, subtracts from, mixes with or keeps the maximum of a light map that starts at the ambient color, and the composite multiplies the scene by the light map. A white ambient color shows the scene as it is, and a dark blue one turns it into a night where only the lights show color. Canvases without ambient light draw as usual, so a HUD on the screen canvas stays unlit.

## What a light reaches

- A light reaches a draw when the light mask of the draw shares a bit with the item mask of the light and the layer of the draw lies in the layer range of the light, so a lamp lights the floor of its room and not the roof above it.
- Draws with a `normalMap` take light by the angle it reaches them at, and `specular` and `shininess` shape their highlights. Draws with `unshaded` keep their colors, and `emission` makes a draw glow on top of the light, which suits eyes, screens and fire.
- The light map holds light in floating point where the backend can render and blend it, as `graphics2d.hdrLighting()` reports, so overlapping lights brighten the scene beyond its unlit colors.

## Shadows

Occluders are outlines that block the light of the lights that cast shadows. An app builds them from points with `lighting2d.newOccluder`, from the shapes of a body of [`haylen.physics2d`](../lua-api/physics2d.md) with `lighting2d.occludersFromBody`, and from the tiles and objects of a map of [`haylen.tiled`](../lua-api/tiled.md) with `lighting2d.occludersFromMap`, so the walls of a level cast shadows without drawing their outlines by hand. Shadow masks choose which occluders block which lights, and soft shadows filter their edges, as [shadows](../lua-api/lighting2d.md#shadows) describes.

## Light in gameplay

The same falloff, cones and shadows answer questions on the CPU: `light:strengthAt` tells how strongly a light reaches a point, `light:shadowedAt` whether occluders hide the point from it, and `lighting2d.illuminate` returns the color that the ambient color and a list of lights leave at a point, as the light pass computes it without normal maps or shadows, which stealth games compare against a threshold. The function `lighting2d.flicker` makes torches and candles waver with smooth noise that gives the same value for the same time and seed, so a replay shows the same flames.

## Performance

Lights draw as instances of one buffer that the frame writes once, so neighbouring lights of one blend mode and texture cost one draw call, and hundreds of lights cost a few. The shadows of every light are cast on the task pool into one atlas that the frame uploads once. Each lit canvas keeps its own render targets at the pixel size of the canvas, so a game lights one world canvas and draws its HUD unlit.

## C++

The C++ types are `haylen::lighting2d::Light`, `Occluder` and `LightFlicker` in `haylen/2d/lighting/`, which `Renderer::drawLight` and `Renderer::drawOccluder` of `haylen::graphics2d` draw into a lit canvas.
