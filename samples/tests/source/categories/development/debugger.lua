-- Debugger: a counter whose function takes a breakpoint of an editor attached to the debugger, a button that halts the app with "debug.breakpoint" and a log of every press the app went on from. The steps say how to attach an editor while the project runs in development, and without an editor the button halts nothing.
local debugging = require('haylen.debug')
local haylen = require('haylen')
local hotReload = require('haylen.hotReload')
local ui = require('haylen.ui')

local Test = require('harness.test')
local LogCard = require('categories.development.log-card')

local Debugger = haylen.class('Debugger', Test)

Debugger.code = [[
local debugging = require('haylen.debug')
function Debugger:tick()  -- A breakpoint here halts once a second.
    self.count = self.count + 1
end
debugging.breakpoint()  -- Halts here while an editor is attached.]]

function Debugger:enter()
    self.count = 0
    self.elapsed = 0
    self.presses = 0
    self.halts = LogCard(self, 'halts', 'Halts', 'Every press of the button shows here once the app goes on.', 'monospace')
    self:frame{
        hint = hotReload.active() and 'Attach an editor to the port that "haylen run" prints, then set a breakpoint or press the button.' or 'The debugger is off, because the project does not run in development. Run it with "haylen run samples/tests" to try it.',
        code = Debugger.code,
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 600, grow = 1, gap = 12,
                ui.label{text = 'Steps', font = 'heading'},
                ui.label{text = '1. Run the project with "haylen run samples/tests".'},
                ui.label{text = '2. Attach the debugger of your editor to the port that the run prints.'},
                ui.label{text = '3. Set a breakpoint in the function "tick" of "source/categories/development/debugger.lua".'},
                ui.label{text = '4. Press the button, which halts where it calls "debug.breakpoint".'},
                ui.label{id = 'ticks', text = 'Ticks: 0', font = 'heading'},
                ui.button{id = 'halt', text = 'Halt here', align = 'stretch', variant = 'primary', onClick = function() self:halt() end},
            },
            self.halts:node(),
        }},
    }
end

-- The function that a breakpoint of the editor halts in, once a second.
function Debugger:tick()
    self.count = self.count + 1
    self:set('ticks', {text = string.format('Ticks: %d', self.count)})
end

function Debugger:update(dt)
    Debugger.super.update(self, dt)
    self.elapsed = self.elapsed + dt
    if self.elapsed >= 1 then
        self.elapsed = self.elapsed - 1
        self:tick()
    end
end

-- Halts while an editor is attached and goes on at once otherwise, so the log shows every press either way.
function Debugger:halt()
    self.presses = self.presses + 1
    debugging.breakpoint()
    self.halts:add(string.format('Press %d', self.presses), 'The app went on after "debug.breakpoint".')
end

return Debugger
