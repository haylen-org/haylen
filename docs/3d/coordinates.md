# 🧭 3D coordinates

This page describes the conventions that every 3D part of Haylen follows: the axes, the units, the angles, the order of matrices, the form of rotations, the winding of triangles, depth and color. Every 3D module, every plugin and every app that works in 3D uses the same conventions, so a position, a rotation or a color means the same thing everywhere. The [reference of `haylen.math`](../lua-api/math.md) lists the types that carry them.

## Axes

The 3D space of Haylen is right-handed. The `x` axis points right, the `y` axis points up and the `z` axis points out of the screen, toward the person who looks at it. A camera without rotation looks along negative `z`, so forward is `(0, 0, -1)`.

```text
          +y (up)
           |
           |
           |
           +-------- +x (right)
          /
         /
       +z (toward the viewer)

   A camera looks along -z, into the screen.
```

Right-handed means that the cross product follows the right hand: curl the fingers from `x` to `y` and the thumb points along `z`. So the cross product of right and up points back toward the camera.

```lua
local m = require('haylen.math')

print(m.right(), m.up(), m.forward()) -- Vec3(1.0, 0.0, 0.0) Vec3(0.0, 1.0, 0.0) Vec3(0.0, 0.0, -1.0)
print(m.right():cross(m.up()) == -m.forward()) -- true
```

The 2D side of Haylen keeps its own axes, with `y` down as on the screen. The two never mix, because 2D and 3D values have their own types, such as `Vec2` and `Vec3`.

## Units

One unit is one meter in physics, audio, XR and light. A character one and a half units tall is one and a half meters tall, gravity pulls at about 9.8 units per second squared, a sound fades with distance in meters, and an XR headset reports the head of the person in meters. Keep the size of things real, and every module behaves the way its numbers say.

## Angles

Angles are in radians, as in 2D. A full turn is `m.tau`, half a turn is `m.pi`, and `m.radians` and `m.degrees` convert. A positive angle around an axis turns counter-clockwise when that axis points at the viewer, by the right hand.

```text
       +y
        |    a positive turn around +z
        |   /
        |  /  turns +x toward +y
        | /
        +--------- +x
   (+z points at the viewer)
```

## Matrices

Matrices are stored by columns, and a matrix multiplies a column vector on its right. So the matrices of a draw chain from right to left, `projection * view * model`: the model matrix moves a point from the space of the object into the world, the view matrix moves it into the space of the camera, and the projection matrix moves it into the clip space of the graphics API.

```text
  object space --model--> world space --view--> camera space --projection--> clip space

  clip = projection * view * model * point
```

```lua
local m = require('haylen.math')

local model = m.compose({0, 0, -5})
local view = m.lookAt({0, 0, 5}, {0, 0, 0})
local projection = m.perspective({fieldOfView = m.radians(60), aspect = 16 / 9})
print((projection * view * model):apply({0, 0, 0})) -- the center of the screen, with its depth
```

## Rotations

Rotations are unit quaternions with the components `(x, y, z, w)`, where `w` is the real part and the identity is `(0, 0, 0, 1)`. Every function that returns a rotation returns it normalized.

```lua
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), m.halfPi)
print(turn, turn:rotate(m.right())) -- Quat(0.0, 0.70710..., 0.0, 0.70710...) close to Vec3(0.0, 0.0, -1.0)
```

## Triangles

The front face of a triangle is the side from which its vertices run counter-clockwise. A model whose scale is negative on one axis, or on three, is a mirror image, which turns its winding around. Its front faces are the clockwise ones, so its triangles keep facing out. The determinant of its matrix is negative, which tells such a model apart.

```text
      2
      /\
     /  \      seen from the front, 0 -> 1 -> 2 runs counter-clockwise
    /____\
   0      1
```

## Depth

Depth is reversed where the graphics API allows it: the near plane has depth `1` and the far plane has depth `0`, in a depth buffer of floats. Floats are densest near zero, so reversed depth keeps far surfaces apart as well as near ones, and the far plane can be infinitely far away.

```lua
local m = require('haylen.math')

local projection = m.perspective({nearPlane = 0.1})
print(projection:apply({0, 0, -0.1}).z, projection:apply({0, 0, -1000}).z) -- 1.0 and close to 0
```

Each graphics API has its own range of depth in clip space. Metal, Direct3D, Vulkan and WebGPU use `0` to `1`, and OpenGL and WebGL2 use `-1` to `1`. The range belongs to the `gpu::Device` of each API, and a projection matrix is built for the range of the device that draws, which the projection functions of `haylen.math` take as `depthRange`. OpenGL takes the range of `0` to `1` with `glClipControl` where the driver has it, and a projection for `-1` to `1` corrects the range elsewhere. Nothing besides the projection and the device converts depth.

Textures and render targets have the same origin on every graphics API, so a texture coordinate and a pixel of a render target point at the same place whatever draws.

## Color

Light adds up in linear space, where twice the number is twice the light. The textures and colors that people pick, the base color and the emissive color of a material, are in sRGB, the encoding of the screen, and the engine decodes them to linear values before light uses them. The data textures of a material, its normals and its occlusion, roughness and metalness (ORM), are linear and never decoded. Tone mapping, which fits the light of a scene to what the screen shows, runs once, at the end of the frame. The [color guide](../graphics/color.md) explains linear light, HDR colors, the color spaces of screens and the physical units of light.
