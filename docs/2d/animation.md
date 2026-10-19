# Animation

The module `haylen.animation2d` moves the images of an app: frame animations cut from sprite sheets and atlases, the animators that play them on sprites, batches that animate whole crowds in C++, state graphs that pick the animation of a character from its parameters, skeletons that animate characters made of parts, and atlases packed while the app runs. This guide explains how the pieces fit together and which one to use. The [`haylen.animation2d` reference](../lua-api/animation2d.md) lists the complete API.

## Frames and atlases

An `Animation` is a list of frames of one texture with a frame rate and a [loop mode](../lua-api/animation2d.md#loop-modes). The function `animation2d.fromGrid` cuts one from a sprite sheet grid and `animation2d.fromFrames` from a list of rectangles, and an atlas exported by an art tool loads with the `atlas` asset type of [`haylen.assets`](../lua-api/assets.md) together with its tags and nine-slices, as [loading sprite atlases](../lua-api/animation2d.md#loading-sprite-atlases) describes.

Frames carry the data that action games read at a given frame: a pivot, named boxes such as hitboxes and hurtboxes, named points such as a hand or a muzzle, named events and a length in ticks for animations that advance by fixed steps, as [frame data](../lua-api/animation2d.md#frame-data) describes. Fighting and platform games step their animations by ticks inside the fixed update, so a hit lands on the same frame on every machine.

## Choosing a player

| Need | Use |
| --- | --- |
| One character whose code picks its animations. | An `Animator`, which plays named animations, queues the next one and reports frames, events and the end of a one-shot animation through `onFrame`, `onEvent` and `onComplete`. |
| Many sprites of one texture, such as a crowd, bullets or coins. | An `AnimatedBatch`, which advances and draws every sprite in C++ with one call from Lua per frame. |
| A character whose animation follows its state, such as idle, run, jump and fall. | A `StateGraph` with a `StateGraphPlayer` for each character, which evaluates the conditions of its transitions in C++ and drives an animator, a sprite of a batch or a track of a skeleton, so only the changes of state reach Lua. |
| A character made of parts, with smooth motion, aiming and equipment. | A `Skeleton` of a `.skeleton` file, with tracks that mix animations, cross fades, inverse kinematics, skins and events. |
| Legs, arms, bodies, tails and tentacles moved by code alone, such as procedural creatures. | A `Chain`, which reaches a target from its root or follows its head, as [procedural animation](#procedural-animation) explains. |

## Skeletons

A `SkeletonData` holds the bones, slots, skins, constraints and animations of a `.skeleton` file and is shared by every `Skeleton` made from it, each with its own pose. Tracks play animations over each other with their own time, loop and weight, inverse kinematics bends a chain of bones toward a target, and events fire as the tracks pass them, reaching listeners of `skeleton:on('event')` as one table. A draw sends the slots as meshes of the atlas texture, so lit canvases light them and neighbouring slots of one blend mode share a draw call. The [skeletons section](../lua-api/animation2d.md#skeletons) of the reference describes the format and every option.

## Procedural animation

A `Chain` animates with code alone, so a creature needs no images or keys for its motion: each leg is a chain that reaches the point where its foot stands, with a `pole` that keeps its knee bending the right way, and a body, a tail or a tentacle is a chain that follows its head. A walking creature keeps its feet planted and moves a foot only when the body has carried its hip too far from it, a few feet at a time, which makes a gait without a walk cycle. The bend limit keeps a body from folding onto itself as it turns, and both ways of bending run in C++ without allocating, so many creatures cost one call per chain each frame. The test project shows a spider and a centipede that climb the walls of a cave this way, as [the reference](../lua-api/animation2d.md#chains) describes.

## Atlases at run time

An `AtlasBuilder` packs images and whole atlases into one texture while the app runs, trims their transparent borders, shares the place of identical frames and pads and extrudes every frame, so sprites that came from many files draw from one texture and batch together, as the [rendering guide](../graphics/rendering.md#textures-and-atlases) explains. Use it for equipment and customization that the app combines at run time, and pack fixed art ahead of time with an art tool.

## Performance

Animated batches and skeletons do their work in C++, a batch advances a large crowd on the job system, and a skeleton allocates nothing once its buffers have grown. The [performance section](../lua-api/animation2d.md#performance) of the reference compares a batch with animators driven from Lua. An app with many animated characters keeps them in batches and leaves animators to the few characters whose code reacts to every frame.

## C++

The C++ types live in `haylen/2d/animation/`: `Animation`, `SpriteFrame`, `Animator`, `AnimatedBatch`, `StateGraph`, `StateGraphPlayer`, `SkeletonData`, `Skeleton`, `SkeletonAnimation`, `SpriteAtlas`, `AtlasBuilder` and `Chain` in the namespace `haylen::animation2d`.
