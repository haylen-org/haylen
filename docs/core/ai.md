# 🧠 AI

This guide explains how to give the characters of an app a mind of their own with the module `haylen.ai`. It covers the four tools of the module: state machines, behavior trees, utility selectors and influence maps. It says what each tool is good at, how it decides, how the tools work together in one character and what they cost. Read it before you write the behavior of an enemy, an ally, a pet or a boss. The [`haylen.ai` reference](../lua-api/ai.md) lists every function, property and error.

## Four ways to decide

Game AI is the code that picks what a character does next. It is rarely clever in the sense of research. It is a set of rules that look believable and stay easy to tune. Each tool of `haylen.ai` shapes those rules in a different way.

| Tool | It answers | Use it for | Avoid it for |
| --- | --- | --- | --- |
| [State machine](../lua-api/ai.md#state-machines) | Which mode is the character in? | Few clear modes, such as idle, patrol and attack, the moves of the player, the phases of a boss and menu flows. | Many modes that each can follow any other, since the changes grow tangled. |
| [Behavior tree](../lua-api/ai.md#behavior-trees) | Which plan works right now? | Plans with steps and fallbacks, such as find cover, else flee, else fight. | Smooth choices between many needs of similar weight. |
| [Utility selector](../lua-api/ai.md#utility-ai) | Which option is worth the most? | Needs that compete, such as hunger, fatigue and fear, or the next action of a unit of a strategy game. | A strict order of steps. |
| [Influence map](../lua-api/ai.md#influence-maps) | Where is the best place? | Danger, control of territory, scents, crowds and the spots where enemies gather. | Who does what, since a map only rates places. |

The tools never run on their own. The engine does not update them, so the app calls them from a scene `update` or `fixedUpdate` with the time step it wants. The functions that a tool calls, such as the `enter` of a state or a leaf of a tree, run inside that call. So they cannot yield a coroutine, and slow work they start goes through [`haylen.jobs`](../lua-api/jobs.md) or a promise instead.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

local bat = {x = 0, awake = false}
bat.mind = ai.newStateMachine({
    sleep = {
        update = function(machine, dt)
            if bat.awake then machine:change('fly') end
        end,
    },
    fly = {
        update = function(machine, dt)
            bat.x = bat.x + 90 * dt
        end,
    },
})
bat.mind:change('sleep')

scene.push({
    update = function(self, dt)
        bat.mind:update(dt)
    end,
})
```

## State machines

A state machine keeps a character in exactly one named state, such as `idle` or `attack`. Each state is a table with up to three functions. The function `enter` runs when the machine arrives in the state, `update` runs on every [`machine:update`](../lua-api/ai.md#machineupdatedt) while the state lasts and `exit` runs when it leaves. A change is one call of [`machine:change`](../lua-api/ai.md#machinechangename-), which can pass extra values to the `enter` of the next state.

The machine counts the seconds in the current state in [`machine.elapsed`](../lua-api/ai.md#machineelapsed), so timed states need no timer of their own. It also remembers the state before in [`machine.previous`](../lua-api/ai.md#machineprevious), which makes a state that returns to where it came from easy to write, such as a stun that ends.

```lua
local ai = require('haylen.ai')

local knight = {stamina = 3}

knight.mind = ai.newStateMachine({
    guard = {
        update = function(machine, dt)
            if knight.hit then
                knight.hit = false
                machine:change('stunned', 0.8)
            end
        end,
    },
    charge = {
        enter = function(machine)
            knight.stamina = knight.stamina - 1
        end,
        update = function(machine, dt)
            if machine.elapsed > 1 then machine:change('guard') end
        end,
    },
    stunned = {
        enter = function(machine, seconds)
            knight.stunTime = seconds
        end,
        update = function(machine, dt)
            if machine.elapsed >= knight.stunTime then
                machine:change(machine.previous)
            end
        end,
    },
})
knight.mind:change('guard')
```

A change that a state function asks for while another change runs waits for it to end, and then runs in the order of the requests. So an `enter` may change again at once, such as a `hurt` state that moves on to `dead` when the health runs out. The function [`machine.onChange`](../lua-api/ai.md#machineonchange) hears every change with its old and new state, which suits animations, sounds and logs that follow the state from one place.

```lua
local ai = require('haylen.ai')

local hero = ai.newStateMachine({idle = {}, run = {}, jump = {}})
local animations = {idle = 'hero-idle', run = 'hero-run', jump = 'hero-jump'}

hero.onChange = function(machine, from, to)
    print(string.format('Play the animation "%s".', animations[to]))
end

hero:change('idle')
hero:change('jump')
```

The machine keeps the table of states and reads each function from it every time it runs one. So code that assigns a new function to a state, such as a tuning tool that tries another behavior, reaches a machine that already runs.

## Behavior trees

A behavior tree describes a plan as a tree of small nodes. The tree walks from its root on every tick, which is one call of [`tree:tick`](../lua-api/ai.md#treetickdt). Each node answers with a status: `'success'`, `'failure'` or `'running'`, where `'running'` means it needs more ticks to finish.

There are three families of nodes.

- Branches decide which children run. A [sequence](../lua-api/ai.md#aisequencechildren) runs its children in order and stops at the first one that fails, like the word "and". A [selector](../lua-api/ai.md#aiselectorchildren) tries its children in order until one succeeds, like the word "or". A [parallel](../lua-api/ai.md#aiparallelchildren-successes) node runs all its children together.
- Decorators wrap one child and change its result or its timing, such as an [inverter](../lua-api/ai.md#aiinverterchild), a [repeater](../lua-api/ai.md#airepeaterchild-count), a [retry](../lua-api/ai.md#airetrychild-count), a [cooldown](../lua-api/ai.md#aicooldownchild-seconds) or a [timeout](../lua-api/ai.md#aitimeoutchild-seconds).
- Leaves do the work. A [condition](../lua-api/ai.md#aiconditionfn) tests a fact, an [action](../lua-api/ai.md#aiactionfn) does something and may take several ticks, and a [wait](../lua-api/ai.md#aiwaitseconds) lets time pass.

Every leaf receives the blackboard, a plain table that the whole tree shares. The blackboard holds what the agent knows, such as its position, its target and its health. Leaves read it and write it, so the nodes of a tree talk to each other through it.

A tree is built in two steps. The node functions, such as `ai.sequence` and `ai.action`, return plain tables that describe the nodes. Then [`ai.newBehaviorTree`](../lua-api/ai.md#ainewbehaviortreeroot-blackboard) reads the whole description once, checks it and builds the tree. Because a description is a plain table, the same branch can appear in many trees, and each place keeps its own progress.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

local function moveTo(key, speed)
    return ai.action(function(board, dt)
        local target = board[key]
        local step = speed * dt
        if math.abs(target - board.x) <= step then
            board.x = target
            return 'success'
        end
        board.x = board.x + (target > board.x and step or -step)
        return 'running'
    end)
end

local guardRoot = ai.selector({
    ai.sequence({
        ai.condition(function(board) return board.health < 30 end),
        moveTo('shelterX', 120),
        ai.action(function(board) board.health = board.health + 1 end),
    }),
    ai.sequence({
        ai.condition(function(board) return board.enemyX ~= nil end),
        moveTo('enemyX', 90),
        ai.cooldown(ai.action(function(board) board.swings = board.swings + 1 end), 0.6),
    }),
    ai.sequence({
        moveTo('postX', 40),
        ai.wait(2),
    }),
})

local guard = ai.newBehaviorTree(guardRoot, {x = 0, health = 100, swings = 0, shelterX = -200, postX = 50})

scene.push({
    update = function(self, dt)
        guard:tick(dt)
    end,
})
```

The selector reads from the top: the first branch is the most urgent plan, and each next branch is the plan for when the ones above it do not apply. Sequences and selectors remember the child that runs, so a long walk goes on from where it was on the next tick instead of starting over. Once the root finishes, the next tick starts it again from its first child.

A tree with only that rule would never notice a new danger while a long action runs, since the sequence goes on from the running child. Two ways keep it alert. A [timeout](../lua-api/ai.md#aitimeoutchild-seconds) bounds how long a child may run. And the app may call [`tree:reset`](../lua-api/ai.md#treereset) when the world changes in a way that matters, such as when the guard hears a noise, so the next tick decides from the top again.

```lua
local ai = require('haylen.ai')

local root = ai.selector({
    ai.condition(function(board) return board.alarm end),
    ai.timeout(ai.action(function(board, dt)
        board.searched = board.searched + dt
        return 'running'
    end), 5),
})
local guard = ai.newBehaviorTree(root, {alarm = false, searched = 0})

guard:tick(1)

local function onNoise()
    guard.blackboard.alarm = true
    guard:reset()
end
onNoise()
print(guard:tick(0)) -- success
```

Time inside a tree is tree time, the sum of the `dt` of every tick. Waits, cooldowns and timeouts all count it, so a tree that ticks at a slower rate, such as a far enemy that thinks four times a second, keeps its timings right.

## Utility selectors

A utility selector picks the option that is worth the most right now. Each option, such as `eat`, `sleep` or `play`, lists its considerations. A consideration reads one input from the context, such as the hunger of a pet, and turns it into a factor from 0 to 1. The score of an option is the product of its factors, times its weight. So an option wins only when every fact that matters for it agrees.

Each consideration first maps its input from its `minimum` and `maximum` to a value from 0 to 1. Then a [response curve](../lua-api/ai.md#response-curves) shapes that value. A straight line means the need grows evenly. A polynomial makes a need matter little at first and a lot near the end, as hunger does. A logistic curve makes a sharp switch around a threshold, as fear of low health does. Tuning the curves is most of the work of a utility AI, and it changes the character of an agent without touching its code.

```lua
local ai = require('haylen.ai')

local pet = {hunger = 7, energy = 2, fun = 5}

local mind = ai.newUtilitySelector({
    {name = 'eat', considerations = {
        {name = 'hunger', input = function(p) return p.hunger end, maximum = 10, curve = {shape = 'polynomial', exponent = 2}},
    }},
    {name = 'sleep', considerations = {
        {name = 'tired', input = function(p) return p.energy end, minimum = 10, maximum = 0, curve = {shape = 'logistic', slope = 10, shift = 0.6}},
    }},
    {name = 'play', weight = 0.7, considerations = {
        {name = 'bored', input = function(p) return p.fun end, minimum = 10, maximum = 0},
        {name = 'awake', input = function(p) return p.energy end, maximum = 10},
    }},
})

local choice, score = mind:choose(pet)
print(string.format('The pet chose to %s with a score of %.2f.', choice, score))
```

A `minimum` above the `maximum` turns an input around, so a low energy maps near 1 for `sleep`. The score also corrects for the number of considerations, so an option with many facts does not lose only because it multiplies more factors.

The best option always wins by default, which can make agents predictable. Pass a [`Random`](../lua-api/math.md#random) to [`selector:choose`](../lua-api/ai.md#selectorchoosecontext-random-tolerance) to pick among the options close to the best one instead, each with a chance that grows with its score. A seeded generator keeps that choice the same in every run, which helps replays and tests. The method [`selector:score`](../lua-api/ai.md#selectorscoreoption-context) gives the score of one option, which suits a debug readout that shows why an agent chose what it chose.

```lua
local ai = require('haylen.ai')
local m = require('haylen.math')

local mind = ai.newUtilitySelector({
    {name = 'wander', considerations = {{input = function(bot) return bot.boredom end}}},
    {name = 'idle', considerations = {{input = function(bot) return bot.calm end}}},
})
local rng = m.random(7)
local bot = {boredom = 0.75, calm = 0.7}

print(mind:choose(bot, rng, 0.85))
for _, name in ipairs(mind.options) do
    print(string.format('%s scores %.2f.', name, mind:score(name, bot)))
end
```

## Influence maps

An influence map lays a grid of numbers over the world. Each cell holds how much of something is there, such as the danger of enemies, the presence of allies or the smell of food. A map answers where questions: where is it safe, where do the enemies gather, which way leads to food. It does not decide who does what, so it works best as an input of another tool, such as a consideration of a utility option or a condition of a tree.

A map changes in a cycle that often runs once per update.

1. Clear it with [`map:fill`](../lua-api/ai.md#mapfillvalue), or fade it with [`map:scale`](../lua-api/ai.md#mapscalefactor) so old influence lingers for a while.
2. Stamp each source with [`map:stamp`](../lua-api/ai.md#mapstampx-y-strength-radius-falloff), a circle of strength that fades toward its edge.
3. Spread the influence with [`map:propagate`](../lua-api/ai.md#mappropagatedecay-momentum), which moves it one cell further on each call. A map that propagates over time behaves like a scent or a rumor that travels around walls of empty cells.
4. Read it with [`map:sample`](../lua-api/ai.md#mapsamplex-y), [`map:findHighest`](../lua-api/ai.md#mapfindhighestx-y-radius) or [`map:findLowest`](../lua-api/ai.md#mapfindlowestx-y-radius).

Several maps combine with [`map:add`](../lua-api/ai.md#mapaddother-weight). A map of allies minus a map of enemies shows who controls each part of the field, with positive cells for one side and negative cells for the other.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

local options = {columns = 40, rows = 24, cellSize = 32}
local threat = ai.newInfluenceMap(options)
local enemies = {{x = 300, y = 200}, {x = 900, y = 500}}
local scout = {x = 640, y = 360}

scene.push({
    update = function(self, dt)
        threat:scale(0.9)
        for _, enemy in ipairs(enemies) do
            threat:stamp(enemy.x, enemy.y, 1, 200, 'quadratic')
        end
        threat:propagate(0.4, 0.7)

        if threat:sample(scout.x, scout.y) > 0.5 then
            local x, y = threat:findLowest(scout.x, scout.y, 250)
            if x then
                scout.goalX, scout.goalY = x, y
            end
        end
    end,
})
```

A map does not move the agent. It names a goal, and the agent reaches it with the paths of [`haylen.navigation2d`](../lua-api/navigation2d.md) or with its own steering. The cell size sets the cost and the detail: a map has `columns` times `rows` cells, and each propagation visits every cell with its eight neighbors. Cells about the size of a character are usually enough. For debugging, [`map:values`](../lua-api/ai.md#mapvalues) returns every cell, which a scene can draw as a heat map.

## Combining the tools

Real characters mix the tools, each at the level it suits. A common layout puts a state machine at the top for the broad modes, a behavior tree or a utility selector inside the mode that needs real decisions, and influence maps under them as shared knowledge about places.

```lua
local ai = require('haylen.ai')
local m = require('haylen.math')

local danger = ai.newInfluenceMap({columns = 32, rows = 18, cellSize = 40})
local rng = m.random(11)

local function newSoldier(x, y)
    local soldier = {x = x, y = y, health = 100, ammo = 6}

    local combat = ai.newUtilitySelector({
        {name = 'shoot', considerations = {
            {input = function(s) return s.ammo end, maximum = 6},
        }},
        {name = 'retreat', considerations = {
            {input = function(s) return danger:sample(s.x, s.y) end, maximum = 2},
            {input = function(s) return s.health end, minimum = 100, maximum = 0},
        }},
    })

    soldier.mind = ai.newStateMachine({
        patrol = {
            update = function(machine, dt)
                if danger:sample(soldier.x, soldier.y) > 0.2 then machine:change('combat') end
            end,
        },
        combat = {
            update = function(machine, dt)
                local choice = combat:choose(soldier, rng, 0.9)
                if choice == 'shoot' then
                    soldier.ammo = soldier.ammo - 1
                elseif choice == 'retreat' then
                    soldier.goalX, soldier.goalY = danger:findLowest(soldier.x, soldier.y, 300)
                end
                if danger:sample(soldier.x, soldier.y) < 0.05 then machine:change('patrol') end
            end,
        },
    })
    soldier.mind:change('patrol')
    return soldier
end

local soldiers = {newSoldier(100, 100), newSoldier(200, 120)}
```

All soldiers here share one map, so the cost of knowing where the danger is does not grow with the number of soldiers. Each soldier keeps its own machine and selector, since those hold the state of one mind.

## Performance

The tools themselves run in C++. A tick of a tree, an update of a machine or a choice of a selector costs little beside the Lua functions that they call, so the cost of an AI is mostly the work of its leaves, states and inputs. Keep those functions short, and read facts that the app already computed, such as distances that physics or [`haylen.spatial2d`](../lua-api/spatial2d.md) found, instead of searching the world in every leaf.

Not every agent needs to think in every frame. A far or hidden agent can tick its tree or choose its option a few times a second, with the time that passed since its last turn as the `dt`. Spread the agents over the frames so they do not all think in the same frame. Influence maps are the heaviest tool, since a propagation visits every cell, so update them at a fixed rate, such as ten times a second, and share them between all the agents that read them.
