-- Containers: how rows, columns, grids, stacks, scrolls, cards, panels, spacers, dividers, tabs, form fields and splitters lay out their children.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Containers = haylen.class('Containers', sample.Test)

Containers.hints = 'Scroll the log with the wheel or a finger, pick tabs, type a name of at least three letters and drag the splitter handle. The arrow keys and the directional pad move between the controls.'
Containers.focus = 'grow-one'

local function cells()
    local list = {}
    for index = 1, 8 do
        list[index] = ui.panel{padding = 12, ui.label{text = 'Cell ' .. index, textAlign = 'center', align = 'stretch'}}
    end
    return list
end

local function logLines()
    local list = {}
    for index = 1, 30 do
        list[index] = ui.label{text = string.format('Day %d: the tide rose %d cm', index, 20 + (index * 37) % 90), color = index % 5 == 0 and 'accentText' or 'text'}
    end
    return list
end

function Containers:content()
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            sample.section('row, column, spacer and divider', {
                ui.row{ui.label{text = 'Start'}, ui.spacer{grow = 1}, ui.label{text = 'A spacer pushes this to the end'}},
                ui.divider{},
                ui.row{gap = 16, ui.label{text = 'Wood 12'}, ui.divider{vertical = true, height = 40, color = 'borderStrong'}, ui.label{text = 'Stone 4'}, ui.divider{vertical = true, height = 40}, ui.label{text = 'Rope 2'}},
                ui.row{gap = 12, ui.button{id = 'grow-one', text = 'grow 1', grow = 1}, ui.button{text = 'grow 2', grow = 2}},
            }),
            sample.section('grid with four columns', {ui.grid{columns = 4, gap = 12, children = cells()}}),
            ui.panel{gap = 8, ui.label{text = 'A panel', font = 'button'}, ui.label{text = 'Panels and cards keep the pointer from reaching the app behind them.', color = 'textMuted'}},
        },
        ui.column{grow = 1, gap = 24,
            sample.section('stack', {
                ui.stack{height = 220,
                    ui.image{image = 'images/landscape_dusk.png', fit = 'cover', align = 'stretch'},
                    ui.label{text = 'Over the picture', font = 'heading', align = 'center', color = 'onAccent', outline = '#FF000000', outlineWidth = 3},
                    ui.badge{text = 'Top right', tone = 'accent', solid = true, align = 'end'},
                },
            }),
            sample.section('scroll', {ui.scroll{id = 'log', height = 260, ui.column{gap = 6, children = logLines()}}}),
        },
        ui.column{grow = 1, gap = 24,
            sample.section('tabs', {
                ui.tabs{id = 'journal', items = {{id = 'quests', text = 'Quests'}, {id = 'map', text = 'Map'}, {id = 'notes', text = 'Notes', enabled = false}}, selected = 'quests', onSelect = function(event)
                    self:setStatus('The tabs picked "' .. event.item .. '"')
                end,
                    ui.label{text = 'Find the lighthouse before the third night.'},
                    ui.image{image = 'images/landscape_day.png', height = 140, fit = 'contain'},
                    ui.label{text = 'Disabled tabs cannot be picked.'},
                },
            }),
            sample.section('formField', {
                ui.formField{id = 'nameField', label = 'Captain', help = 'Shown on the leaderboard', required = true,
                    ui.textField{id = 'captain', placeholder = 'Your name', maxLength = 16, onChange = function(event)
                        event.document:set('nameField', {error = #event.value < 3 and 'Use at least three letters' or ''})
                    end},
                },
            }),
            sample.section('splitter', {
                ui.splitter{id = 'split', height = 200, ratio = 0.35, onResize = function(event)
                    self:setStatus(string.format('splitter ratio %.2f', event.ratio))
                end,
                    ui.list{items = {{id = 'north', text = 'North'}, {id = 'south', text = 'South'}, {id = 'east', text = 'East'}}},
                    ui.label{text = 'The first child takes the ratio of the width, and the handle between them moves it.', color = 'textMuted'},
                },
            }),
        },
    }
end

return Containers
