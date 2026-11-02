-- Labels in every font role and theme color, alignment, wrapping and outlines, page headers, section titles, empty states and alerts.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Text = haylen.class('Text', Test)

Text.code = [[
ui.label{text = 'Outlined', font = 'heading', color = 'onAccent', outline = '#FF1E1B33', outlineWidth = 2}
ui.label{text = 'Center', textAlign = 'center', align = 'stretch'}
ui.pageHeader{title = 'Page header', caption = 'With a banner', banner = true, textAlign = 'center'}
ui.emptyState{image = 'interface/themes/fantasy/icons/key.png', title = 'Nothing here', message = 'Open a chest to find items.'}
ui.alert{tone = 'warning', title = 'Warning', message = 'Night is coming.'}]]

Text.long = 'A long hint that wraps onto several lines because the label is narrower than its text.'

local function roles()
    local list = {gap = 6}
    for index, role in ipairs({'title', 'heading', 'button', 'body', 'caption', 'monospace'}) do
        list[index] = ui.label{text = 'Role "' .. role .. '"', font = role}
    end
    return list
end

-- Each label names the theme color it is written in.
local function colors()
    local list = {gap = 6}
    for index, role in ipairs({'text', 'textMuted', 'accentText', 'successText', 'warningText', 'dangerText', 'informationText'}) do
        list[index] = ui.label{text = '"' .. role .. '"', color = role}
    end
    return list
end

function Text:enter()
    self:frame{
        hint = 'Labels take a font role and a theme color, so a theme restyles every text at once. Rich text has its own test.',
        code = Text.code,
        content = {layout.columns{
            ui.column{grow = 1, gap = 24,
                layout.section('Font roles', roles()),
                layout.section('Option "textAlign"', {gap = 10,
                    ui.panel{padding = {6, 20}, ui.label{text = 'Start', textAlign = 'start', align = 'stretch'}},
                    ui.panel{padding = {6, 20}, ui.label{text = 'Center', textAlign = 'center', align = 'stretch'}},
                    ui.panel{padding = {6, 20}, ui.label{text = 'End', textAlign = 'end', align = 'stretch'}},
                }),
            },
            ui.column{grow = 1, gap = 24,
                layout.section('Theme colors', colors()),
                ui.pageHeader{title = 'Page header', caption = 'With a banner and centered text', banner = true, textAlign = 'center'},
            },
            ui.column{grow = 1, gap = 24,
                layout.section('Wrap and outline', {
                    ui.label{text = Text.long, align = 'stretch'},
                    ui.column{gap = 4, align = 'stretch',
                        ui.label{text = Text.long, wrap = false, align = 'stretch'},
                        ui.label{text = 'With "wrap = false", a line that does not fit ends in an ellipsis.', font = 'caption', color = 'textMuted', align = 'stretch'},
                    },
                    ui.stack{height = 110,
                        ui.image{image = 'interface/images/landscape.png', fit = 'cover', align = 'stretch', radius = 12},
                        ui.label{text = 'Outlined over a picture', font = 'heading', align = 'center', color = 'onAccent', outline = '#FF1E1B33', outlineWidth = 2},
                    },
                }),
                ui.alert{tone = 'warning', title = 'Warning', message = 'Night is coming.'},
                ui.alert{tone = 'danger', title = 'Danger', message = 'The raft is sinking.'},
            },
            ui.column{grow = 1, gap = 24,
                layout.section('Component "emptyState"', {ui.emptyState{image = 'interface/themes/fantasy/icons/key.png', imageSize = 96, title = 'Nothing here', message = 'Open a chest to find items.'}}),
                ui.alert{tone = 'information', title = 'Information', message = 'The ferry leaves at noon.'},
                ui.alert{tone = 'success', title = 'Success', message = 'Your progress was saved.'},
            },
        }},
    }
end

return Text
