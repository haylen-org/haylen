# Haylen Physics

A Lua sample with one scene per feature of [`haylen.physics2d`](../../../docs/lua-api/physics2d.md). The menu lists the tests, each test opens as its own scene with a Back button, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu.

| Test | What it shows |
| --- | --- |
| Bodies and shapes | Boxes, circles, capsules, convex and concave polygons on dynamic bodies, a segment and a chain on static ones, and a mouse joint that drags them. |
| Materials | Friction on a ramp, restitution of bouncing balls and density on a seesaw. |
| Sensors and contacts | A sensor zone that counts its visitors, and the contact begin, contact end, hit and sensor events of the world. |
| Joints | Distance joints with a spring and with limits, a revolute motor, a revolute with limits, a prismatic lift, soft welds, a wheel on suspension, a motor joint and a filter joint. |
| Ropes | Ropes pinned at one or both ends, hanging from bodies or tying two bodies together, and cut in the middle. |
| Bridge | A bridge of planks between two cliffs under crates, barrels and a heavy block. |
| Ragdolls | Human figures with limited joints and joint friction, falling down stairs. |
| Vehicle | A car on springy suspension with rear, front or all wheel drive on a hilly road. |
| Explosions | Radial impulses with no, linear or quadratic falloff and occlusion behind walls. |
| Destructible terrain | Ground carved by bombs and a shovel and filled by a trowel, with its collision rebuilt chunk by chunk. |
| Fracture | Objects that break into Voronoi pieces around the point they are hit, by a click or a wrecking ball. |
| Liquids | A particle fluid poured into a tank with floating crates, drawn as metaballs. |
| One-way platforms | A character that jumps up through platforms, lands on them, drops through them and rides a moving one. |
| Conveyors | Belts that carry crates with the tangent speed of their surface. |
| Ray and shape casts | Closest and all hits, category filters and accept functions, piercing, bounces, fans, circle, box, capsule and polygon casts, a batch of 720 rays, picking and line of sight. |
| Collision filtering | Categories, masks and positive and negative groups. |
| Stress test | Hundreds of bodies drawn with sprite batches, with the body count and the step time. |

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 haylen.py run samples/gameplay/physics` |
| macOS app | `python3 haylen.py run samples/gameplay/physics --platform macos` |
| iPhone and iPad simulator | `python3 haylen.py run samples/gameplay/physics --platform ios-simulator` |
| Apple TV simulator | `python3 haylen.py run samples/gameplay/physics --platform tvos-simulator` |
| Android device or emulator | `python3 haylen.py run samples/gameplay/physics --platform android --device <serial>` |
| Browser | `python3 haylen.py run samples/gameplay/physics --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
| Drag, blast, carve, pour and aim | Left mouse button | Right stick moves a cursor, right trigger presses | Finger | Buttons of the panel |
| Start the test over | R | West button | Button of the panel | Button of the panel |
| Next or previous cast | E and Q | Shoulder buttons | Radio buttons | Radio buttons |
| Drive and walk | A and D or the arrows, Space to jump | Left stick or triggers, south button to jump | On-screen pedals, stick and jump button | Swipes |

The options panel of each test takes the focus when the test opens, so gamepads and TV remotes reach every option with the directional pad. The driving and walking tests leave the focus free, so the arrows drive instead of moving between options.
