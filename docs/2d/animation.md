# 🎞️ Animation

This guide explains how to animate 2D images with the module `haylen.animation2d`. It is for developers who animate characters, effects and animated UI. It covers frame animations from sprite sheets and atlases and the data that frames carry for action games. It also covers the players that run animations, state graphs that pick an animation from the state of a character, skeletons for characters made of parts, chains for procedural motion and atlases packed while the app runs. The [`haylen.animation2d` reference](../lua-api/animation2d.md) lists every function, option and error.

## Choosing a player

An animation is only data: a list of frames. A player runs it over time and shows the current frame. Pick the player that fits:

| Need | Use |
| --- | --- |
| One character whose code picks its animations. | An `Animator`, which plays named animations, queues the next one and reports frames, events and the end of a one-shot animation. |
| Many sprites of one texture, such as a crowd, bullets or coins. | An `AnimatedBatch`, which advances and draws every sprite in C++ with one call from Lua per frame. |
| A character whose animation follows its state, such as idle, run, jump and fall. | A `StateGraph` with a `StateGraphPlayer` for each character. It drives an animator, a sprite of a batch or a track of a skeleton, so only the changes of state reach Lua. |
| A character made of parts, with smooth motion, aiming and equipment. | A `Skeleton` of a `.skeleton` file, with tracks that mix animations, cross fades, inverse kinematics, skins and events. |
| Legs, arms, bodies, tails and tentacles moved by code alone, such as procedural creatures. | A `Chain`, which reaches a target from its root or follows its head. |

## Frame animations

An `Animation` is a list of frames of one texture. Each frame is a rectangle of the texture and lasts some time. The animation also has a loop mode:

- `'loop'` starts again after the last frame.
- `'once'` stops on the last frame and reports that it finished.
- `'pingPong'` plays forward and then backward, without showing the end frames twice in a row.

A sprite sheet is one image that holds the frames of an animation in a grid of cells of the same size. The function `animation2d.fromGrid` cuts an animation from it. Cells count from 1, left to right and top to bottom, and the option `cells` picks and orders them. The function `animation2d.fromFrames` builds an animation from a list of rectangles instead, for frames of different sizes.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')

local sheet = assets.texture('sprites/knight/attack.png')
local idle = animation2d.fromGrid(assets.texture('sprites/knight/idle.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 10})
local slash = animation2d.fromGrid(sheet, {frameWidth = 192, frameHeight = 192, cells = {1, 2, 3, 4}, framesPerSecond = 12, loop = 'once'})
local flame = animation2d.fromFrames(assets.texture('effects/fire.png'), {{0, 0, 64, 64}, {64, 0, 64, 64}, {128, 0, 64, 64}}, {loop = 'pingPong'})
print(idle.frameCount, slash.duration, flame.cycleDuration)
```

## Sprite atlases

A sprite atlas is one image that packs many frames of different sizes, with a JSON file that names each frame. Art tools export atlases, and an atlas keeps the art of a whole character, or a whole game, in one texture. Load one with the `atlas` asset type of [`haylen.assets`](../lua-api/assets.md), which returns a `SpriteAtlas`.

The JSON of an atlas carries more than the frames:

- Frame tags become named animations, with the duration of each frame. Read one with `atlas:animation(name)`.
- Slices with a center become nine-slices for panels and buttons. Read one with `atlas:slice(name)`.
- Other slices become frame data, the boxes and points described below.

Packers often trim the transparent border of each frame to save room. The atlas remembers where each trimmed frame sat in its original frame, so pivots stay still while the animation plays and the character does not shake.

The method `atlas:apply(sprite, name)` shows one named frame on a sprite, such as an icon. The [reference](../lua-api/animation2d.md#atlas-files) lists the fields of the JSON file.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')

local atlas = assets.load('units/hero.json', 'atlas', {filter = 'linear'})
local animator = animation2d.newAnimator()
for _, name in ipairs(atlas:animationNames()) do
    animator:add(name, atlas:animation(name))
end
local icon = graphics2d.newSprite(atlas.texture, {x = 64, y = 64})
atlas:apply(icon, 'hero 0')
local panel = atlas:slice('panel')
```

## Frame data

Action games read data at given frames, such as the frame where a sword can hit. Each frame of an animation can carry:

- A pivot, the point the frame stands on, such as the feet of a character.
- Boxes, named rectangles such as a hitbox that deals damage and a hurtbox that takes it.
- Points, named spots such as a hand that holds a weapon or the muzzle of a gun.
- Events, names that fire when the animation enters the frame, such as `'step'` for a footstep sound or `'swing'` for the start of a hit.
- Ticks, the length of the frame in fixed steps, as the next sections explain.

An atlas reads frame data from its slices. The function `animation2d.fromFrames` takes it as tables, and `anim:addEvent` adds events in code. The methods `animator:box` and `animator:point` place a box or a point in the world. They go through the sprite that shows the frame, so they follow its position, scale, rotation and flips.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('fighters/brawler.json', 'atlas')
local fighter = graphics2d.newSprite(atlas.texture, {x = 400, y = 500, flipHorizontal = true})
local animator = animation2d.newAnimator()
animator:add('punch', atlas:animation('punch'))
animator.onEvent = function(name, event, frame)
    if event == 'swing' then
        local hitbox = animator:box('hit', fighter)
        print(frame, hitbox and hitbox.width)
    end
end
animator:play('punch')

scene.push({
    update = function(self, dt)
        animator:update(dt)
        animator:apply(fighter)
    end,
})
```

## Animators

An `Animator` holds named animations and plays one at a time on a sprite. Each frame, call `animator:update(dt)` to advance it and `animator:apply(sprite)` to show the current frame. The animator changes only the texture, source rectangle, size and pivot of the sprite. Its position, scale, rotation, color and flips stay yours.

The pivot of a frame keeps a character standing on the same spot. Frames without a pivot use `animator.pivotX` and `animator.pivotY`, a fraction of the untrimmed frame. When the sprite flips, the pivot flips with it, so a character that turns around stays on its feet.

- `animator:play(name)` switches to an animation. Playing the current one again keeps its time, unless you ask for a restart.
- `animator:queue(name)` plays an animation after the current one ends its pass, such as an idle after an attack.
- `animator:stop()` pauses it, and `play` with the same name resumes it.
- `animator.speed` plays faster or slower.

The callbacks `onFrame`, `onEvent` and `onComplete` report each new frame, each event of a frame and the end of a one-shot animation. They run inside `animator:update`, in order, even for the frames that a long update skips over.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local knight = 'sprites/knight/'
local hero = graphics2d.newSprite(assets.texture(knight .. 'idle.png'), {x = 400, y = 400})
local animator = animation2d.newAnimator()
animator:add('idle', animation2d.fromGrid(assets.texture(knight .. 'idle.png'), {frameWidth = 192, frameHeight = 192}))
animator:add('attack', animation2d.fromGrid(assets.texture(knight .. 'attack.png'), {frameWidth = 192, frameHeight = 192, framesPerSecond = 12, loop = 'once'}))
animator.onComplete = function(name)
    animator:play('idle')
end
animator:play('idle')

scene.push({
    update = function(self, dt)
        if input.pressed('attack') then
            animator:play('attack', true)
        end
        animator:update(dt)
        animator:apply(hero)
    end,
    render = function(self)
        graphics2d.beginScreen()
        hero:draw()
    end,
})
```

## Fixed steps and ticks

Fighting and platform games run their logic in fixed steps, so a hit lands on the same frame on every machine. For them, an animation can count ticks instead of seconds: each frame lasts a whole number of fixed steps. Give `ticksPerFrame` to `animation2d.fromGrid`, or `ticks` to each frame of `animation2d.fromFrames`.

Advance such an animation with `animator:step(ticks)` inside the fixed update of the scene, never with `animator:update(dt)`. Every frame of an animation counts ticks, or none does.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local scene = require('haylen.scene')

local sheet = assets.texture('fighters/brawler.png')
local animator = animation2d.newAnimator()
animator:add('jab', animation2d.fromGrid(sheet, {frameWidth = 240, frameHeight = 240, cells = {1, 2, 3}, ticksPerFrame = 2, loop = 'once'}))
animator:play('jab')

scene.push({
    fixedUpdate = function(self, step)
        animator:step(1)
    end,
})
```

## Animated batches

An animator costs a few calls from Lua in every frame, which adds up for a crowd of hundreds. An `AnimatedBatch` is a [sprite batch](../lua-api/graphics2d.md#spritebatch) whose sprites each play an animation from a shared set, with their own time, speed and scale. The batch advances every sprite in C++, so a crowd of thousands costs one call to update and one call to draw. Batches of 4096 sprites or more advance on the worker pool.

Sprites in a batch are numbered from 1. Removing a sprite renumbers the sprites after it. Listen to the `'complete'` event to learn which sprites finished a one-shot animation, once per update.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local atlas = assets.load('units/villager.json', 'atlas')
local crowd = animation2d.newAnimatedBatch(atlas.texture, {walk = atlas:animation('walk'), idle = atlas:animation('idle')})
for index = 1, 2000 do
    crowd:add({x = math.random(0, 4000), y = math.random(0, 3000), animation = index % 3 == 0 and 'idle' or 'walk', speed = 0.8 + math.random() * 0.4})
end
local camera = graphics2d.newCamera()

scene.push({
    update = function(self, dt)
        crowd:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {sort = 'y'})
        crowd:draw({layer = 1})
    end,
})
```

## State graphs

A character often has many animations, and the code that picks the right one grows messy. A state graph moves that choice into data. It has:

- Parameters, values that the game sets, such as `speed`, `grounded` or `facing`. A parameter is a number, a boolean, a trigger or a vector. A trigger is a one-time signal, such as an attack button press.
- States, such as `idle`, `run` and `slash`. Each state plays one animation, or one animation for each direction.
- Transitions, which move from one state to another when their condition holds, such as `'speed > 0.1'`.

The `StateGraph` is the checked description, shared by every character. Each character gets its own `StateGraphPlayer`, which keeps its parameter values and its current state. The game sets parameters, and the player switches animations on its target: an animator, one sprite of an animated batch or a track of a skeleton. The player never advances the target, so you still update the animator or the batch yourself. The player evaluates conditions in C++, so only the changes of state reach Lua, through its `'stateChanged'` event.

On each update, a player takes at most one transition. It tries the transitions in order, and the first one that may fire wins. A transition may fire when:

- It starts from the current state, or from `'any'` state.
- Its exit time has passed. The exit time is the share of one pass of the animation of the state that must have played. With `atEnd = true` the state waits for the end of its animation.
- Its condition holds. A transition consumes the triggers its condition reads, so one press fires one attack.

A directional state picks the animation of the direction its vector points to, in screen coordinates where `{0, 1}` points down. It takes four or eight directions. To avoid flicker on the diagonals, it keeps its direction while the vector stays a little past the edge of the sector, within a quarter of half a sector. It also keeps its direction while the vector is zero, so a character that stops keeps facing the same way. A turn keeps the time of the animation, so the steps of a walk stay in rhythm.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local input = require('haylen.input')
local scene = require('haylen.scene')

local graph = animation2d.newStateGraph({
    parameters = {speed = 0, attack = 'trigger', facing = {0, 1}},
    states = {
        idle = {directions = {down = 'idleDown', up = 'idleUp', left = 'idleLeft', right = 'idleRight'}, by = 'facing'},
        run = {directions = {down = 'runDown', up = 'runUp', left = 'runLeft', right = 'runRight'}, by = 'facing'},
        slash = {animation = 'slash'},
    },
    transitions = {
        {from = 'idle', to = 'run', when = 'speed > 0.1'},
        {from = 'run', to = 'idle', when = 'speed <= 0.1'},
        {from = 'any', to = 'slash', when = 'attack'},
        {from = 'slash', to = 'idle', atEnd = true},
    },
})
local atlas = assets.load('units/hero.json', 'atlas')
local animator = animation2d.newAnimator()
for _, name in ipairs(atlas:animationNames()) do
    animator:add(name, atlas:animation(name))
end
local player = graph:newPlayer(animator)

scene.push({
    update = function(self, dt)
        local x, y = input.vector('move')
        player:set('speed', math.sqrt(x * x + y * y))
        player:set('facing', {x, y})
        if input.pressed('attack') then
            player:trigger('attack')
        end
        player:update(dt)
        animator:update(dt)
    end,
})
```

A graph can also live in a JSON file with the same keys, which `animation2d.newStateGraph` loads by its package path.

## Skeletons

A skeleton animates a character made of parts, such as cut-out pieces or bendable meshes, on a hierarchy of bones. One small set of images then gives smooth motion, many animations, equipment swaps and aiming. A `.skeleton` file describes the character in JSON, as the [reference](../lua-api/animation2d.md#skeleton-files) describes.

The parts of a skeleton work like this:

- Bones form a tree, like the nodes of a [node tree](nodes.md): each bone sits on its parent. The `x`, `y`, `rotation`, `scaleX` and `scaleY` of the skeleton place the root bone in the world.
- A slot sits on a bone and shows one attachment at a time. An attachment is a region of the atlas, a mesh whose vertices follow one bone or several bones by weight, a point such as a hand, or an outline. Slots draw in order, and animations can change that order.
- A skin is a set of attachments, such as the blue armor or the red armor. The skin `default` always exists, and another skin falls back to it for the slots it leaves out.
- Animations turn, move and scale bones from their setup pose. They also color slots, swap attachments, bend meshes, change constraints, reorder slots and fire events.

The shared `SkeletonData` of a file holds the bones, slots, skins, constraints and animations. Each `Skeleton` made from it has its own pose, so many characters share one file.

Tracks let animations mix. Each track plays one animation with its own time, loop and weight, and later tracks play over earlier ones. A walk on track 1 and a wave on track 2 mix into a walking wave. The method `skeleton:crossFade` blends the pose of the old animation of a track into the new one over some seconds, so changes look smooth.

Constraints bend bones toward goals:

- Inverse kinematics bends one bone, or a bone and its child, so the end of the chain reaches a target. The target is a bone, or a point that the app gives with `skeleton:setIkTarget`, such as the mouse for an arm that aims.
- Transform constraints make bones follow the rotation and the place of another bone.

Events fire as the tracks pass them, and reach the listeners of `skeleton:on('event')` as one table. A state graph can drive a track, and players on several tracks of one skeleton make layers, such as legs that walk on one track while the arms aim on another.

A draw sends the slots in draw order as meshes of the atlas texture. Neighbouring slots of one blend mode share a draw call, and lit canvases light them like other meshes. Posing and skinning run in C++ on the frame thread, and nothing allocates once the buffers of a skeleton have grown.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local camera = graphics2d.newCamera()
local knight = animation2d.newSkeleton(assets.load('characters/knight.skeleton'), {skin = 'blue', x = 0, y = 200})
knight:play('walk', {track = 1})

scene.push({
    enter = function(self)
        knight:on('event', function(event) if event.name == 'step' then print('Footstep.') end end, {owner = self})
    end,
    update = function(self, dt)
        if input.pressed('attack') then
            knight:crossFade('slash', 0.1, {track = 2, loop = false})
        end
        knight:setIkTarget('aim', camera:screenToWorld(input.mousePosition()))
        knight:update(dt)
    end,
    render = function(self)
        graphics2d.beginWorld(camera, {sort = 'y'})
        knight:draw({layer = 2})
    end,
})
```

## Procedural animation with chains

A `Chain` animates with code alone, so a creature needs no images or keys for its motion. A chain is a row of points joined by segments of fixed lengths. It bends in two ways:

- `chain:reach(x, y)` keeps the first point at the root and bends the chain so its last point reaches a target. This suits legs and arms. A `pole` is a point on the side the joints bend toward, such as a point above the knee, so a knee bends the right way.
- `chain:follow(x, y)` moves the first point as a head and drags every other point behind it. This suits bodies, tails, tentacles and ropes, which follow the path of their head like a snake or a centipede.

The `maxBend` limits the bend between two segments in a row, which keeps a body from folding onto itself as it turns. The method `chain:setPoint` places one point, such as a point of a body pushed out of a wall, and the next `reach` or `follow` fixes the lengths from there.

A walking creature keeps its feet planted. It moves a foot only when the body has carried the hip too far from it, a few feet at a time, which makes a gait without a walk cycle. Both ways of bending run in C++ without allocating, so many creatures cost one call per chain in each frame. The test project shows a spider and a centipede that climb the walls of a cave this way.

```lua
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local leg = animation2d.newChain({x = 400, y = 200, lengths = {90, 90}})
local tail = animation2d.newChain({x = 200, y = 300, segments = 24, length = 14, angle = math.pi, maxBend = 0.5})

scene.push({
    update = function(self, dt)
        local x, y = input.mousePosition()
        leg:reach(x, y, {pole = {400, 0}})
        tail:follow(x, y)
    end,
    render = function(self)
        graphics2d.beginScreen()
        for index = 1, tail.count - 1 do
            local x1, y1 = tail:point(index)
            local x2, y2 = tail:point(index + 1)
            graphics2d.drawLine(x1, y1, x2, y2, 18 - index * 0.5, '#FF35B37E')
        end
    end,
})
```

## Atlases at run time

Sprites draw together in one batch only when they share a texture. Art that comes from many files, such as equipment that the player picks, would break batches. An `AtlasBuilder` packs images and whole atlases into one texture while the app runs, so they draw from one texture and batch together, as the [rendering guide](../graphics/rendering.md#textures-and-atlases) explains.

When it packs, the builder:

- Trims the transparent border of every frame, and keeps where the frame sat in its untrimmed frame, so its pivot stays.
- Shares one place between frames that show the same pixels.
- Pads every frame and repeats its edge pixels outward, so neighbours never bleed into each other when the texture is scaled.
- Bleeds the colors of an atlas with the `'linear'` filter into its transparent pixels, so edges stay clean.

The method `builder:addAtlas` adds a whole atlas with its animations under a name prefix, and `atlas:subset(prefix)` gives back the part of one character. The method `builder:buildAsync` packs on the task pool without holding up the frame. Use a builder for art that the app combines while it runs, and pack fixed art ahead of time with an art tool.

```lua
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local async = require('async')

async.spawn(function()
    local builder = animation2d.newAtlasBuilder()
    for _, name in ipairs({'knight', 'archer'}) do
        local atlas = assets.loadAsync('units/' .. name .. '.json', 'atlas', {filter = 'linear'}):await()
        builder:addAtlas(name .. '/', atlas, assets.loadAsync('units/' .. name .. '.png', 'image'):await())
    end
    local packed = builder:buildAsync({padding = 4, extrude = 2, filter = 'linear'}):await()
    local knight = packed:subset('knight/')
    print(#packed:frameNames(), knight:animation('run').frameCount)
end)
```

## Performance

Animated batches and skeletons do their work in C++. A batch advances a large crowd on the job system, and a skeleton allocates nothing once its buffers have grown. An app with many animated characters keeps them in batches, and keeps animators for the few characters whose code reacts to every frame.

The command `haylen bench --suite animation` measures this on the headless host. These are the numbers of an Apple M5 Pro in Release with 15 workers:

| Scene | Average ms per frame |
| --- | --- |
| Animated batch of 100,000 sprites, one thread | 0.930 |
| Animated batch of 100,000 sprites, job system | 0.191 |
| 2,000 units with one animator each, updated and applied from Lua | 0.232 |
| 2,000 units of one animated batch, updated from Lua | 0.017 |

## From C++

The C++ types live in `haylen/2d/animation/`, in the namespace `haylen::animation2d`: `Animation`, `SpriteFrame`, `Animator`, `AnimatedBatch`, `StateGraph`, `StateGraphPlayer`, `SkeletonData`, `Skeleton`, `SkeletonAnimation`, `SpriteAtlas`, `AtlasBuilder` and `Chain`.

```cpp
#include <any>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"

class Runner final : public haylen::core::Scene {
  public:
    void enter(haylen::core::Engine& engine, const std::any& params) override {
        const haylen::graphics::Texture sheet = engine.getAssets().texture("sprites/knight/run.png");
        animator.add("run", haylen::animation2d::Animation::fromGrid(sheet, {.frameSize = {192.0F, 192.0F}, .framesPerSecond = 12.0F}));
        animator.play("run");
        hero.texture = sheet;
    }

    void update(haylen::core::Engine& engine, float deltaSeconds) override {
        animator.update(deltaSeconds);
        animator.apply(hero);
    }

    void render(haylen::core::Engine& engine) override {
        engine.getRenderer2D().beginWorld(camera);
        engine.getRenderer2D().draw(hero);
    }

  private:
    haylen::animation2d::Animator animator;
    haylen::graphics2d::Sprite hero;
    haylen::graphics2d::Camera camera;
};
```
