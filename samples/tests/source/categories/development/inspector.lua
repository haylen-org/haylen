-- Inspector: a player table and a camera that the test watches, listed and edited through the inspector, with an edit that a setter refuses, and the inspector window.
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Inspector = haylen.class('Inspector', Test)

function Inspector:enter()
    self.lines = {}
    self.player = {speed = 220, name = 'Hero', alive = true, items = {'axe', 'rope'}}
    self.lens = graphics2d.newCamera()
    debugging.watch('player', self.player)
    debugging.watch('camera', self.lens)
    self:frame{
        hint = 'Open the inspector and edit "watched.player.speed", or type "inspect watched.player" in the console.',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.label{id = 'state', text = self:describe(), font = 'heading'},
                ui.button{id = 'open', text = 'Open the inspector', align = 'stretch', variant = 'primary', onClick = function() self:open() end},
                ui.button{id = 'edit', text = 'Set the speed to 300', align = 'stretch', onClick = function() self:run('set watched.player.speed 300') end},
                ui.button{id = 'refuse', text = 'Set the smallest zoom to -1', align = 'stretch', onClick = function() self:run('set watched.camera.minZoom -1') end},
                ui.label{font = 'monospace', text = "debug.watch(name, value)\ndebug.inspect(path)\ndebug.setInspectorVisible(true)"},
            },
            ui.scroll{grow = 1, ui.label{id = 'log', text = '', grow = 1}},
        }},
    }
    local keys = {}
    for _, entry in ipairs(debugging.inspect('watched.player')) do
        keys[#keys + 1] = entry.key .. ' (' .. entry.type .. ')'
    end
    self:note('The player holds ' .. table.concat(keys, ', ') .. '.')
    self:note(string.format('The camera lists %d properties.', #debugging.inspect('watched.camera')))
    self:run('set watched.player.name Brave Hero')
end

function Inspector:exit()
    debugging.unwatch('player')
    debugging.unwatch('camera')
    if debugging.inspectorVisible() then
        debugging.setInspectorVisible(false)
    end
    Inspector.super.exit(self)
end

function Inspector:update(dt)
    Inspector.super.update(self, dt)
    local text = self:describe()
    if text ~= self.shown then
        self.shown = text
        self:set('state', {text = text})
    end
end

function Inspector:describe()
    return string.format('%s runs at %d, %s', self.player.name, self.player.speed, self.player.alive and 'alive' or 'down')
end

function Inspector:note(line)
    table.insert(self.lines, 1, line)
    self.lines[13] = nil
    self:set('log', {text = table.concat(self.lines, '\n')})
end

function Inspector:run(line)
    local ok, output = debugging.runCommand(line)
    self:note(string.format('%s "%s": %s', ok and 'Ran' or 'Refused', line, output))
end

function Inspector:open()
    if not debugging.consoleEnabled() then
        self:note('The inspector is off, because the app runs outside development without "debug.console" in "app.json".')
        return
    end
    debugging.setInspectorVisible(true)
end

return Inspector
