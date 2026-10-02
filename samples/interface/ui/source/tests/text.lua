-- Text: labels in every font role and color, alignment, wrapping and outlines, page headers, section titles, empty states and alerts.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Text = haylen.class('Text', sample.Test)

Text.hints = 'Labels take a font role and a theme color, so a theme restyles every text at once. Rich text has its own test.'

local kLong = 'A long hint that wraps onto several lines because the label is narrower than its text.'

local function roles()
    local list = {}
    for index, role in ipairs({'title', 'heading', 'button', 'body', 'caption', 'monospace'}) do
        list[index] = ui.label{text = 'Role "' .. role .. '"', font = role}
    end
    return list
end

local function colors()
    local list = {}
    for index, role in ipairs({'text', 'textMuted', 'accentText', 'successText', 'warningText', 'dangerText', 'informationText'}) do
        list[index] = ui.label{text = 'Color "' .. role .. '"', color = role}
    end
    return ui.grid{columns = 2, gap = 8, children = list}
end

function Text:content()
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            sample.section('Font roles', roles()),
            sample.section('Theme colors', {colors()}),
        },
        ui.column{grow = 1, gap = 24,
            sample.section('Option "textAlign"', {
                ui.panel{padding = 8, ui.label{text = 'Start', textAlign = 'start', align = 'stretch'}},
                ui.panel{padding = 8, ui.label{text = 'Center', textAlign = 'center', align = 'stretch'}},
                ui.panel{padding = 8, ui.label{text = 'End', textAlign = 'end', align = 'stretch'}},
            }),
            sample.section('Wrap and outline', {
                ui.label{text = kLong, width = 480},
                ui.label{text = kLong, wrap = false, width = 480},
                ui.label{text = 'Outlined over the app', font = 'heading', color = 'onAccent', outline = '#FF000000', outlineWidth = 4},
            }),
        },
        ui.column{grow = 1, gap = 24,
            ui.pageHeader{title = 'Page header', caption = 'With a banner and centered text', banner = true, textAlign = 'center'},
            sample.section('Component "emptyState"', {ui.emptyState{image = 'icons/key.png', imageSize = 96, title = 'Nothing here', message = 'Open a chest to find items.'}}),
            ui.alert{tone = 'information', title = 'Information', message = 'The ferry leaves at noon.'},
            ui.alert{tone = 'success', title = 'Success', message = 'Your progress was saved.'},
            ui.alert{tone = 'warning', title = 'Warning', message = 'Night is coming.'},
            ui.alert{tone = 'danger', title = 'Danger', message = 'The raft is sinking.'},
        },
    }
end

return Text
