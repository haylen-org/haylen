# 🏃 3D animation

This guide explains how Haylen animates 3D models and scenes with the module `haylen.animation3d`. It covers tracks, the keys of one property over time, the four ways values run between keys, clips that hold the tracks of one animation, the clips of glTF files, players that run a clip over time and report its events, and the edits that make new clips. The [`haylen.animation3d` reference](../lua-api/animation3d.md) lists every function, option and error.

## Tracks

A track holds the keys of one property of one target. A key is a time in seconds and a value. The target names a node or a bone, such as `Hips`, and the property names what moves:

- `position`, `rotation` and `scale` move the transform of a node or a bone.
- `weights` moves the morph weights of a mesh.
- Any other path names a property of a component of the target, such as `light.intensity` or `material.baseColor`. The part of the engine that plays the track on the target reads the path.

Each track holds values of one type: a number, a `vec2`, a `vec3` or a `vec4`, a `quat` for rotations, a `color` in linear light, the `weights` of the morph targets of a mesh with one number per target, a `boolean` or a `string`. Booleans and strings change only by steps, so they suit things that switch, such as whether a node shows or which face a sprite of a character wears.

```lua
local animation3d = require('haylen.animation3d')
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), math.pi / 2)
local spin = animation3d.newTrack({target = 'Turret', property = 'rotation', type = 'quat', times = {0, 1}, values = {0, 0, 0, 1, turn.x, turn.y, turn.z, turn.w}})
local glow = animation3d.newTrack({target = 'Lamp', property = 'light.intensity', times = {0, 0.5, 1}, values = {1, 4, 1}})
local face = animation3d.newTrack({target = 'Hero', property = 'face', type = 'string', interpolation = 'step', times = {0, 0.8}, strings = {'calm', 'angry'}})
print(spin:sample(0.5), glow:sample(0.25), face:sample(1))
```

A track never changes once made, and every edit returns a new track. A time before the first key gives the first value and a time after the last key gives the last value. Two keys at one time make an instant jump: the later key shows from that time on.

## Interpolation

The interpolation of a track says how values run between keys:

- `step` holds the value of a key until the next key.
- `linear` runs in a straight line. A rotation runs along the shorter arc between its keys at a constant speed, so a turn never takes the long way around.
- `cubicSpline` is the cubic Hermite spline of glTF. Each key holds an in tangent, its value and an out tangent, in units per second, so a curve leaves and reaches each key at the speed its tangents give. A rotation becomes a unit quaternion again after each sample.
- `bezier` runs along a cubic Bezier curve through the handles of each key, as the curve editors of animation tools draw them. A handle is a time and a value as offsets from its key: the in handle comes at or before its key, and the out handle at or after it, each within the time to the key next to it, so the curve never goes back in time.

```lua
local animation3d = require('haylen.animation3d')

-- Each key gives four numbers per component: the time and value of its in handle, then of its out handle.
local ease = animation3d.newTrack({target = 'Door', property = 'angle', interpolation = 'bezier', times = {0, 2}, values = {0, 1.5}, handles = {0, 0, 0.6, 0, -0.6, 0, 0, 0}})
print(ease:sample(0.2), ease:sample(1), ease:sample(1.8))
```

Sampling is deterministic: one time always gives the same value, whatever was sampled before, and a time on a key gives the value of that key exactly. A sample finds its key from the key of the last sample in a step or two, so a clip that plays forward or backward costs no search.

## Clips

A clip holds the tracks of one animation, such as a walk or the opening of a door, and named events at moments of it, such as each footstep. Each property of a target has one track in a clip. A clip lasts until its last key or event, or for the duration it names.

A clip samples every track at once into one float buffer, the components of each track one after another in the order of the tracks. The method `clip:offset` gives the place of the first number of a track. So a clip of a hundred bones samples in one call, and the code that applies the values reads the buffer.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')

local open = animation3d.newClip({
    name = 'open',
    tracks = {
        animation3d.newTrack({target = 'Door', property = 'angle', times = {0, 1}, values = {0, 1.5}}),
        animation3d.newTrack({target = 'Door', property = 'position', type = 'vec3', times = {0, 1}, values = {0, 0, 0, 0, 0.1, 0}}),
    },
    events = {{time = 0.9, name = 'creak', data = {volume = 0.5}}},
})
local values = collections.newFloatBuffer(open.valueCount)
open:sample(0.5, values)
print(values[open:offset('Door', 'angle')])
```

## Clips of glTF files

The animations of a [glTF file](gltf.md) become clips with `animation3d.clipFromGltf` and `animation3d.clipsFromGltf`. Each channel of an animation becomes a track, with the times, values and interpolation of its sampler: translation becomes `position`, rotation `rotation`, scale `scale` and the morph weights `weights`.

A track names its node by the name of the node in the file, so a clip of one file moves the nodes of the same names in another file, such as a walk exported alone that plays on every character of the same rig. A node without a name, or whose name another node of the file shares, takes its glTF pointer instead, such as `/nodes/3`, which names it by its place in the file. The reader checks the data of every animation, and the clip checks again what it reads, so a document made in code cannot make a clip of hostile data either.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
for _, clip in ipairs(animation3d.clipsFromGltf(document)) do
    print(clip.name, clip.duration, #clip.tracks)
end
local wave = animation3d.clipFromGltf(document, document.animations[1])
```

## Players

A `ClipPlayer3D` runs one clip over time. Each `player:update(dt)` moves its time by `dt` times its speed, samples the clip into its values and reports the events it passed. Its loop mode says what happens at the ends:

- `'loop'` starts again from the other end, so a walk repeats without end.
- `'once'` stops at the end and reports `'complete'`, such as an attack.
- `'pingPong'` turns and plays the other way, such as a pendulum.

A negative speed plays backward, and the player wraps or turns at the start the same way. A paused player keeps its time and reports nothing. Setting `player.time` moves the player without passing the events between, and the next update reports the events at that time.

Events reach the listeners of `player:on('event', listener, {owner = owner})` as `{name, time, data}`, in the order the player passes them, on the frame thread inside the update. A listener ends with its owner, so nothing runs for a scene that is gone. An update far longer than the clip, such as after the app was in the background, reports the events of the last three passes it makes, as the 2D skeletons do.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')
local scene = require('haylen.scene')

local document = gltf.read('models/robot.glb')
local walk = animation3d.clipFromGltf(document, document.animations[1])
local player = animation3d.newClipPlayer(walk, {loop = 'loop', speed = 1.2})

scene.push({
    enter = function(self)
        player:on('event', function(event)
            print('Footstep of the foot ' .. event.data.foot .. '.')
        end, {owner = self})
    end,
    update = function(self, dt)
        player:update(dt)
        local hips = player:value('Hips', 'position')
    end,
})
```

## Editing clips

Clips are made and edited in Lua and in C++, and every edit returns a new clip:

- `clip:trimmed(start, end)` keeps the part between two times, moved to start at zero, with its events. The curves stay exact, since the trim adds keys at both ends that keep the shape of each curve.
- `clip:joined(other)` plays another clip after this one, joining the tracks of the same property of the same target.
- `clip:resampled(framesPerSecond)` gives every track that interpolates keys at a fixed rate, while tracks of steps keep their keys.
- `clip:optimized(tolerance)` drops the keys that a line between the kept keys brings back within the tolerance, in the units of each track and in radians for rotations.
- `clip:compressed(tolerance)` drops keys the same way and keeps values in 16-bit steps of the range of each track where that stays within the tolerance, which takes about half the memory.
- `clip:toAdditive(reference)` makes the clip relative to the pose of another clip, which a layer adds over another animation, such as a wave over a walk. Positions and other values become differences, scales ratios and rotations turns from the reference.
- `animation3d.clipFromMorphSequence(target, targetCount)` plays the morph targets of a mesh one after another, each one fully at its frame, such as the frames of a mouth that a tool baked as targets.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local all = animation3d.clipFromGltf(document, document.animations[1])
local intro = all:trimmed(0, 1.5):renamed('intro')
local light = all:resampled(30):compressed(0.001)
local wave = animation3d.clipFromGltf(document, document.animations[2]):toAdditive(all, {time = 0})
print(intro.duration, light.memorySize, all.memorySize, wave.name)
```

## From C++

The C++ types live in `haylen/3d/animation/`, in the namespace `haylen::animation3d`: `Track3D`, `Clip3D` and `ClipPlayer3D`. A clip is shared data, so players hold it as a `std::shared_ptr<const Clip3D>`, and its sample writes into a span of floats with a cursor for each track. Sampling allocates nothing, and an update of a player allocates nothing once its events of one update fit what earlier updates held.

```cpp
#include <memory>
#include <vector>

#include "haylen/3d/animation/ClipPlayer3D.hpp"
#include "haylen/3d/gltf/Reader.hpp"

const haylen::gltf::Document document = haylen::gltf::Reader::read(engine.getPackage(), "models/robot.glb");
auto walk = std::make_shared<const haylen::animation3d::Clip3D>(haylen::animation3d::Clip3D::fromGltf(document, 0));
haylen::animation3d::ClipPlayer3D player(walk, {.loop = haylen::animation3d::ClipPlayer3D::Loop::Loop, .speed = 1.2F});
player.eventFired.connect([](const haylen::animation3d::Clip3D::Event& event) { haylen::core::Log::info("{}", event.name); });
player.update(1.0F / 60.0F);
const std::span<const float> values = player.getValues();
```
