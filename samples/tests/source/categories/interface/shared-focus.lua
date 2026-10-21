-- A page and a player bar mounted as GUIs of their own that share the focus, so a remote or a gamepad moves between them, a sheet that covers the content of a scroll and a form that keeps its maximum width inside a scroll.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local SharedFocus = haylen.class('SharedFocus', Test)

SharedFocus.code = [[
local page = ui.mount(pageTree, {owner = self, sharedFocus = true})
local bar = ui.mount(ui.panel{anchor = 'bottomRight', margin = 24, ui.button{id = 'play', text = 'Play'}}, {owner = self, layer = 1, sharedFocus = true})
ui.panel{id = 'sheet', anchor = 'stretchBottom', height = 220, focusScope = true, onCancel = close}  -- It comes after the scroll, so it covers it.
ui.scroll{grow = 1, height = 0, ui.column{maxWidth = 560, children = rows}}]]

SharedFocus.tracks = {'Morning tide', 'Paper lanterns', 'Slow river', 'Copper sky'}
SharedFocus.options = {'Show the grid of the map', 'Mute the sound of the waves', 'Sail at night', 'Light the lanterns at dusk', 'Keep the hold locked', 'Trade with the merchants', 'Fish on the way', 'Wake at dawn', 'Follow the gulls', 'Chart new islands', 'Avoid the reefs', 'Ring the bell in port'}

function SharedFocus:enter()
    self.track = 1
    self.playing = false
    self:frame{
        hint = 'Press down from the form to reach the bar at the bottom right, a GUI of its own that shares the focus with this page. The sheet covers the scrolling form.',
        code = SharedFocus.code,
        focus = 'name',
        content = {ui.row{gap = 24, alignItems = 'start', self:form(), self:playlist()}},
        overlay = {self:sheet()},
    }
    self.gui.sharedFocus = true
    self.bar = ui.mount(self:barTree(), {owner = self, layer = 1, sharedFocus = true})
    self:keepInRegion(self.bar, 'bar', 24)
    self:refresh()
end

function SharedFocus:form()
    local rows = {}
    for index, text in ipairs(SharedFocus.options) do
        rows[index] = ui.checkbox{id = 'option' .. index, text = text, checked = index % 3 == 1}
    end
    return ui.card{grow = 1,
        ui.column{maxWidth = 560, gap = 12,
            ui.sectionTitle{text = 'A form of 560 units at most'},
            ui.formField{label = 'Name of the captain', ui.textField{id = 'name', value = 'Rui'}},
            ui.button{id = 'open', text = 'Open the sheet', variant = 'primary', onClick = function() self:setSheet(true) end},
            ui.column{gap = 8, children = rows},
        },
    }
end

-- The tracks of the bar, which a pick plays, above the bar in its corner.
function SharedFocus:playlist()
    local items = {}
    for index, track in ipairs(SharedFocus.tracks) do
        items[index] = {id = 'track-' .. index, text = track, caption = string.format('Track %d', index), image = 'audio/cover.png', iconColor = false}
    end
    return ui.card{grow = 1, gap = 12,
        ui.sectionTitle{text = 'Playlist'},
        ui.list{id = 'playlist', items = items, selected = 'track-1', onSelect = function(event)
            self.track = tonumber(event.item:match('%d+'))
            self.playing = true
            self:refresh()
        end},
    }
end

function SharedFocus:sheet()
    return ui.panel{id = 'sheet', visible = false, anchor = 'stretchBottom', anchorTo = 'parent', height = 220, focusScope = true, onCancel = function() self:setSheet(false) end,
        ui.column{gap = 12,
            ui.label{text = 'This sheet comes after the scroll in the tree, so it covers the form.', font = 'heading'},
            ui.label{text = 'The focus stays in the sheet until it closes.', color = 'textMuted'},
            ui.button{id = 'close', text = 'Close the sheet', onClick = function() self:setSheet(false) end},
        },
    }
end

function SharedFocus:barTree()
    return ui.panel{id = 'bar', anchor = 'bottomRight', margin = 24, padding = 12,
        ui.row{gap = 12,
            ui.image{image = 'audio/cover.png', width = 64, height = 64, fit = 'cover', radius = 10},
            ui.column{width = 230, gap = 2, ui.label{id = 'track', text = '', wrap = false}, ui.label{id = 'state', text = '', font = 'caption', color = 'textMuted'}},
            ui.button{id = 'previous', text = 'Previous', onClick = function() self:skip(-1) end},
            ui.button{id = 'play', text = 'Play', variant = 'primary', width = 120, onClick = function() self:toggle() end},
            ui.button{id = 'next', text = 'Next', onClick = function() self:skip(1) end},
        },
    }
end

function SharedFocus:setSheet(open)
    self.gui:set('sheet', {visible = open})
    self.gui:command(open and 'close' or 'open', 'focus')
end

function SharedFocus:skip(step)
    self.track = (self.track + step - 1) % #SharedFocus.tracks + 1
    self:refresh()
end

function SharedFocus:toggle()
    self.playing = not self.playing
    self:refresh()
end

function SharedFocus:refresh()
    self.bar:set('play', {text = self.playing and 'Pause' or 'Play'})
    self.gui:set('playlist', {selected = 'track-' .. self.track})
    self.bar:set('track', {text = SharedFocus.tracks[self.track]})
    self.bar:set('state', {text = string.format('%s, track %d of %d', self.playing and 'Playing' or 'Paused', self.track, #SharedFocus.tracks)})
    self:status(string.format('The bar %s "%s", track %d of %d.', self.playing and 'plays' or 'paused', SharedFocus.tracks[self.track], self.track, #SharedFocus.tracks))
end

return SharedFocus
