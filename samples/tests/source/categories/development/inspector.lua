-- Inspector: a player table and a camera that the test watches, listed and edited through the inspector, with an edit that a setter refuses, and the inspector window. A table shows what the inspector lists for the player as it changes, and the card beside it the edits with their results.
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local LogCard = require('categories.development.log-card')

local Inspector = haylen.class('Inspector', Test)

Inspector.code = [[
local player = {speed = 220, name = 'Hero', alive = true}
debugging.watch('player', player)  -- Held weakly under the root "watched".
for _, entry in ipairs(debugging.inspect('watched.player')) do print(entry.key, entry.type) end
debugging.runCommand('set watched.player.speed 300')
debugging.setInspectorVisible(true)]]

function Inspector:enter()
    self.player = {speed = 220, name = 'Hero', alive = true, items = {'axe', 'rope'}}
    self.lens = graphics2d.newCamera()
    self.edits = LogCard(self, 'edits', 'Edits', 'The edits of the buttons show here.', 'monospace')
    debugging.watch('player', self.player)
    debugging.watch('camera', self.lens)
    self:frame{
        hint = 'Open the inspector and edit "watched.player.speed", or type "inspect watched.player" in the console.',
        code = Inspector.code,
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 600, gap = 12,
                ui.label{text = 'The player in the inspector', font = 'heading'},
                ui.table{id = 'player', columns = {{text = 'Key', width = 130}, {text = 'Type', width = 150}, {text = 'Value'}}, rows = self:rows()},
                ui.label{text = string.format('The camera lists %d properties.', #debugging.inspect('watched.camera')), color = 'textMuted'},
                ui.button{id = 'open', text = 'Open the inspector', align = 'stretch', variant = 'primary', onClick = function() self:open() end},
                ui.button{id = 'edit', text = 'Set the speed to 300', align = 'stretch', onClick = function() self:run('set watched.player.speed 300') end},
                ui.button{id = 'refuse', text = 'Try a zoom limit of -1', align = 'stretch', onClick = function() self:run('set watched.camera.minZoom -1') end},
            },
            self.edits:node(),
        }},
    }
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

-- The entries of the player as rows of the table.
function Inspector:rows()
    local rows = {}
    for _, entry in ipairs(debugging.inspect('watched.player')) do
        rows[#rows + 1] = {id = tostring(entry.key), cells = {tostring(entry.key), entry.type, entry.value}}
    end
    return rows
end

-- Lists the player again once a value changed, from the inspector or from a button.
function Inspector:update(dt)
    Inspector.super.update(self, dt)
    local text = string.format('%s %d %s', self.player.name, self.player.speed, tostring(self.player.alive))
    if text ~= self.shown then
        self.shown = text
        self:set('player', {rows = self:rows()})
    end
end

function Inspector:run(line)
    local ok, output = debugging.runCommand(line)
    self.edits:add('> ' .. line, ok and output or ('Refused: ' .. output), not ok and 'danger' or nil)
end

function Inspector:open()
    if not debugging.consoleEnabled() then
        self.edits:add('The inspector is off', 'The app runs outside development without "debug.console" in "app.json".', 'danger')
        return
    end
    debugging.setInspectorVisible(true)
end

return Inspector
