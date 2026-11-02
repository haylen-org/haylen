-- The events every kind reports: a card that the pointer hovers, presses, drags and scrolls, and that shows, hides, leaves and comes back, with every event it reports in a log.
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local NodeEvents = haylen.class('NodeEvents', Test)

NodeEvents.code = [[
local note = function(event) log.info(event.name) end  -- Such as "press", "drag", "hide" or "unmount".
ui.card{id = 'box', width = 320, height = 200, onMount = note, onUnmount = note, onShow = note, onHide = note,
    onHover = note, onPress = note, onRelease = note, onScroll = note, onDrag = function(event)
        local shape = event.gui:transform('box')
        shape.offset = shape.offset + m.vec2(event.deltaX, event.deltaY)
    end}]]

NodeEvents.logSize = 10
-- The tone of the badge of each event in the log.
NodeEvents.tones = {mount = 'success', unmount = 'danger', show = 'success', hide = 'warning', hover = 'information', press = 'accent', drag = 'accent', release = 'accent', scroll = 'information'}

function NodeEvents:init(entry)
    NodeEvents.super.init(self, entry)
    self.lines = {}
    self.generation = 1
end

function NodeEvents:enter()
    self:frame{
        hint = 'Hover, press, drag and scroll the card with the mouse or a finger. The buttons hide it, show it and replace it, and the log lists every event it reports in order.',
        code = NodeEvents.code,
        focus = 'hide',
        content = {layout.columns{
            layout.section('The card', {grow = 1, align = 'stretch',
                ui.stack{id = 'room', height = 380, self:card()},
                ui.row{wrap = true, gap = 12,
                    ui.button{id = 'hide', text = 'Hide', onClick = function() self:set('box', {visible = false}) end},
                    ui.button{text = 'Show', onClick = function() self:set('box', {visible = true}) end},
                    ui.button{text = 'Replace', onClick = function() self:replace() end},
                    ui.button{text = 'Clear the log', variant = 'link', onClick = function() self:clear() end},
                },
            }),
            layout.section('Events in order', {grow = 1, align = 'stretch', ui.column{id = 'log', gap = 10, align = 'stretch'}}),
        }},
    }
end

function NodeEvents:card()
    local note = function(event) self:note(event) end
    return ui.card{id = 'box', width = 340, align = 'center', padding = {12, 20, 20, 20}, gap = 8, alignItems = 'center', style = {colors = {raised = '#FF39405C', border = '#FF8FB0FF'}},
        onMount = note, onUnmount = note, onShow = note, onHide = note, onHover = note, onPress = note, onRelease = note, onScroll = note,
        onDrag = function(event)
            local shape = event.gui:transform('box')
            shape.offset = shape.offset + m.vec2(event.deltaX, event.deltaY)
            self:note(event)
        end,
        ui.icon{image = 'interface/icons/grip.svg', size = 32, color = 'textMuted'},
        ui.image{image = 'interface/images/island-map.png', height = 110, fit = 'cover', radius = 10, align = 'stretch'},
        ui.label{text = string.format('Map %d', self.generation), font = 'heading', textAlign = 'center', align = 'stretch'},
        ui.label{text = 'Drag me around', color = 'textMuted', textAlign = 'center', align = 'stretch'},
    }
end

function NodeEvents:replace()
    self.generation = self.generation + 1
    self.gui:replaceChildren('room', {self:card()})
end

function NodeEvents:clear()
    self.lines = {}
    self.gui:replaceChildren('log', {})
end

-- Describes an event with its values in a sentence.
function NodeEvents.describe(event)
    if event.name == 'hover' then
        return event.hovered and 'The pointer came over the card.' or 'The pointer left the card.'
    elseif event.name == 'press' then
        return string.format('The %s button pressed at %.0f, %.0f.', event.button, event.x, event.y)
    elseif event.name == 'drag' then
        return string.format('The %s button dragged it to %.0f, %.0f.', event.button, event.x, event.y)
    elseif event.name == 'release' then
        return string.format('The %s button let go %s the card.', event.button, event.inside and 'inside' or 'outside')
    elseif event.name == 'scroll' then
        return string.format('A scroll of %.1f across and %.1f down.', event.deltaX, event.deltaY)
    end
    return ({mount = 'The card joined the GUI.', unmount = 'The card left the GUI.', show = 'The card became visible.', hide = 'The card was hidden.'})[event.name]
end

-- Writes an event as a row of the log, the newest at the bottom, merging the drags of one gesture into one row.
function NodeEvents:note(event)
    local line = {name = event.name, text = NodeEvents.describe(event)}
    local last = self.lines[#self.lines]
    if event.name == 'drag' and last and last.name == 'drag' then
        self.lines[#self.lines] = line
    else
        self.lines[#self.lines + 1] = line
    end
    while #self.lines > NodeEvents.logSize do
        table.remove(self.lines, 1)
    end
    if self.gui and self.gui:has('log') then
        local rows = {}
        for index, entry in ipairs(self.lines) do
            rows[index] = ui.row{gap = 16, ui.badge{text = entry.name:sub(1, 1):upper() .. entry.name:sub(2), tone = NodeEvents.tones[entry.name], width = 120}, ui.label{text = entry.text, color = 'textMuted', grow = 1}}
        end
        self.gui:replaceChildren('log', rows)
    end
    self:set('status', {text = string.format('Map %d reported the event "%s".', self.generation, event.name)})
end

return NodeEvents
