-- The events every kind reports: a card that the pointer hovers, presses, drags and scrolls, and that shows, hides, leaves and comes back, with every event it reports in a log.
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local NodeEvents = haylen.class('NodeEvents', Test)

NodeEvents.logSize = 12

function NodeEvents:init(entry)
    NodeEvents.super.init(self, entry)
    self.lines = {}
    self.generation = 1
end

function NodeEvents:enter()
    self:frame{
        hint = 'Hover, press, drag and scroll the card with the mouse or a finger. The buttons hide it, show it and replace it, and the log lists every event it reports in order.',
        focus = 'hide',
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('The card', {ui.stack{id = 'room', height = 420, self:card()}}),
                ui.row{gap = 12,
                    ui.button{id = 'hide', text = 'Hide', onClick = function() self:set('box', {visible = false}) end},
                    ui.button{text = 'Show', onClick = function() self:set('box', {visible = true}) end},
                    ui.button{text = 'Replace', onClick = function() self:replace() end},
                    ui.button{text = 'Clear the log', variant = 'link', onClick = function() self:clear() end},
                },
            },
            ui.column{grow = 1, gap = 12, layout.section('Events', {ui.label{id = 'log', text = '', font = 'monospace'}})},
        }},
    }
end

function NodeEvents:card()
    local note = function(event) self:note(event) end
    return ui.card{id = 'box', width = 320, height = 200, align = 'center', padding = 20,
        onMount = note, onUnmount = note, onShow = note, onHide = note, onHover = note, onPress = note, onRelease = note, onScroll = note,
        onDrag = function(event)
            local shape = event.gui:transform('box')
            shape.offset = shape.offset + m.vec2(event.deltaX, event.deltaY)
            self:note(event)
        end,
        ui.label{text = string.format('Card %d', self.generation), font = 'heading', textAlign = 'center', align = 'stretch'},
        ui.label{text = 'Drag me', color = 'textMuted', textAlign = 'center', align = 'stretch'},
    }
end

function NodeEvents:replace()
    self.generation = self.generation + 1
    self.gui:replaceChildren('room', {self:card()})
end

function NodeEvents:clear()
    self.lines = {}
    self:set('log', {text = ''})
end

-- Writes an event with its values, merging the drags of one gesture into one line.
function NodeEvents:note(event)
    local text
    if event.name == 'hover' then
        text = string.format('Hover %s', tostring(event.hovered))
    elseif event.name == 'press' or event.name == 'drag' then
        text = string.format('%s %s at %.0f, %.0f', event.name == 'press' and 'Press' or 'Drag', event.button, event.x, event.y)
    elseif event.name == 'release' then
        text = string.format('Release %s, inside %s', event.button, tostring(event.inside))
    elseif event.name == 'scroll' then
        text = string.format('Scroll %.1f, %.1f', event.deltaX, event.deltaY)
    else
        text = event.name:sub(1, 1):upper() .. event.name:sub(2)
    end
    if event.name == 'drag' and self.lines[#self.lines] and self.lines[#self.lines]:find('^Drag') then
        self.lines[#self.lines] = text
    else
        self.lines[#self.lines + 1] = text
    end
    while #self.lines > NodeEvents.logSize do
        table.remove(self.lines, 1)
    end
    if self.gui and self.gui:has('log') then
        self:set('log', {text = table.concat(self.lines, '\n')})
    end
    self:set('status', {text = string.format('Card %d reported "%s".', self.generation, event.name)})
end

return NodeEvents
