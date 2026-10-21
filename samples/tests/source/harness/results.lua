-- Rows of checks of a test: a state, a name and a detail. The states are waiting, pass, fail, info and skip, which marks what does not apply to the platform, such as an unsupported call. A test shows them as cards in a panel of its GUI, or draws them on its stage. Every row that settles in a new state goes to the log with the code of the test, and a row that fails publishes the event "testCheckFailed", which fails the test in a run of every test.
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Results = haylen.class('Results')

Results.colors = {waiting = Test.warm, pass = Test.green, fail = Test.red, info = Test.accent, skip = Test.muted}
Results.marks = {waiting = 'Waiting', pass = 'Pass', fail = 'Fail', info = 'Info', skip = 'N/A'}

-- The cards of the rows: the tone of the badge of each state, the color of its icon and the icon itself, while a row that waits shows a spinning ring.
Results.tones = {waiting = 'warning', pass = 'success', fail = 'danger', info = 'information', skip = 'neutral'}
Results.inks = {waiting = 'warningText', pass = 'successText', fail = 'dangerText', info = 'informationText', skip = 'textMuted'}
Results.icons = {pass = 'interface/states/pass.svg', fail = 'interface/states/fail.svg', info = 'interface/states/info.svg', skip = 'interface/states/skip.svg'}
Results.iconSize = 36

-- The layout of the rows on a stage: the margin around them, the gap between two columns, where names and details start after the state, the height of a name line, the space after a row and the size of the details.
Results.margin = 24
Results.gap = 32
Results.indent = 140
Results.line = 32
Results.spacing = 24
Results.detailSize = 19

function Results:init(code)
    self.code = code
    self.rows = {}
    self.byKey = {}
end

-- Adds the row of `key`, or changes it, and shows the change on the cards once the panel is shown.
function Results:set(key, state, name, detail)
    local row = self.byKey[key]
    local added = row == nil
    if added then
        row = {}
        self.byKey[key] = row
        self.rows[#self.rows + 1] = row
        row.id = 'check-' .. #self.rows
    end
    local settled = state ~= 'waiting' and row.state ~= state
    local before = {state = row.state, name = row.name, detail = row.detail}
    row.state, row.name, row.detail = state, name, tostring(detail or '')
    if settled then
        local write = state == 'fail' and log.warning or log.info
        write(string.format('[%s] %s: %s. %s', self.code, Results.marks[state], name, row.detail))
    end
    if settled and state == 'fail' then
        events.emit('testCheckFailed', {code = self.code, message = name .. ': ' .. row.detail})
    end
    if not self:shown() then
        return
    end
    if added then
        self.gui:replaceChildren('checks-rows', self:cards())
    else
        self:refreshCard(row, before)
    end
    self:showSummary()
end

function Results:clear()
    self.rows = {}
    self.byKey = {}
    if self:shown() then
        self.gui:replaceChildren('checks-rows', {})
        self:showSummary()
    end
end

-- Sets the row from a failed call: an unsupported call does not apply to the platform, and any other failure fails the row.
function Results:failure(key, name, err)
    if err.code == 'unsupported' then
        self:set(key, 'skip', name, 'Unsupported on this platform: ' .. err.message)
    else
        self:set(key, 'fail', name, string.format('%s (code "%s")', err.message, tostring(err.code)))
    end
end

function Results:count(state)
    local count = 0
    for _, row in ipairs(self.rows) do
        if row.state == state then
            count = count + 1
        end
    end
    return count
end

-- Tells how the checks stand in a sentence, such as "4 of 5 checks passed, and 1 is waiting.", and returns an empty text while there is nothing to check, which hides the status.
function Results:summary()
    local checks = #self.rows - self:count('info')
    if checks == 0 then
        return ''
    end
    local parts = {string.format('%d of %d %s passed', self:count('pass'), checks, checks == 1 and 'check' or 'checks')}
    local failed, waiting, skipped = self:count('fail'), self:count('waiting'), self:count('skip')
    if failed > 0 then
        parts[#parts + 1] = string.format('%d failed', failed)
    end
    if waiting > 0 then
        parts[#parts + 1] = string.format('%d %s waiting', waiting, waiting == 1 and 'is' or 'are')
    end
    if skipped > 0 then
        parts[#parts + 1] = string.format('%d %s not apply here', skipped, skipped == 1 and 'does' or 'do')
    end
    if #parts == 1 then
        return parts[1] .. '.'
    end
    return table.concat(parts, ', ', 1, #parts - 1) .. (#parts > 2 and ',' or '') .. ' and ' .. parts[#parts] .. '.'
end

-- The badge over the cards: what passed, or what failed or waits, in the tone of the worst state.
function Results:badge()
    local checks = #self.rows - self:count('info')
    if checks == 0 then
        return '', 'neutral'
    end
    local failed, waiting = self:count('fail'), self:count('waiting')
    if failed > 0 then
        return string.format('%d failed', failed), 'danger'
    end
    if waiting > 0 then
        return string.format('%d waiting', waiting), 'warning'
    end
    if self:count('pass') == 0 then
        return 'Not applicable', 'neutral'
    end
    return string.format('%d of %d passed', self:count('pass'), checks), 'success'
end

-- The panel of the checks for the GUI of the test: a title, the badge and the cards in a scroll that fills the room the panel gets, or the text `options.empty` under a mark while there is no row. Call `show` with the mounted GUI so the cards follow the rows.
function Results:view(title, options)
    options = options or {}
    local text, tone = self:badge()
    return ui.panel{id = 'checks', grow = options.grow or 1, width = options.width, align = 'stretch', gap = 16,
        ui.row{gap = 12,
            ui.sectionTitle{text = title or 'Checks', grow = 1},
            ui.badge{id = 'checks-summary', text = text, tone = tone, visible = text ~= ''},
        },
        ui.column{id = 'checks-empty', grow = 1, gap = 16, justify = 'center', align = 'stretch', visible = #self.rows == 0,
            ui.icon{image = Results.icons.info, size = 72, color = 'textMuted', align = 'center'},
            ui.label{text = options.empty or 'Nothing is checked yet.', color = 'textMuted', textAlign = 'center', align = 'center', maxWidth = 420},
        },
        ui.scroll{id = 'checks-scroll', grow = 1, height = 0, visible = #self.rows > 0, ui.column{id = 'checks-rows', gap = 12, children = self:cards()}},
    }
end

function Results:show(gui)
    self.gui = gui
end

function Results:shown()
    return self.gui ~= nil and self.gui.mounted
end

function Results:cards()
    local cards = {}
    for index, row in ipairs(self.rows) do
        cards[index] = self:card(row)
    end
    return cards
end

function Results:card(row)
    local id = row.id
    return ui.card{id = id, padding = {16, 20},
        ui.row{gap = 18, alignItems = 'start',
            ui.stack{width = Results.iconSize, height = Results.iconSize,
                ui.icon{id = id .. '-icon', image = Results.icons[row.state], size = Results.iconSize, color = Results.inks[row.state], visible = row.state ~= 'waiting'},
                ui.busyIndicator{id = id .. '-busy', size = Results.iconSize, color = 'warning', visible = row.state == 'waiting'},
            },
            ui.column{grow = 1, gap = 4,
                ui.label{id = id .. '-name', text = row.name},
                ui.label{id = id .. '-detail', text = row.detail, font = 'caption', color = 'textMuted', visible = row.detail ~= ''},
            },
            ui.badge{id = id .. '-state', text = Results.marks[row.state], tone = Results.tones[row.state]},
        },
    }
end

-- Changes only the parts of a card whose row changed, since a row that counts frames changes its detail every frame.
function Results:refreshCard(row, before)
    local id = row.id
    if row.state ~= before.state then
        self.gui:set(id .. '-icon', {image = Results.icons[row.state], color = Results.inks[row.state], visible = row.state ~= 'waiting'})
        self.gui:set(id .. '-busy', {visible = row.state == 'waiting'})
        self.gui:set(id .. '-state', {text = Results.marks[row.state], tone = Results.tones[row.state]})
    end
    if row.name ~= before.name then
        self.gui:set(id .. '-name', {text = row.name})
    end
    if row.detail ~= before.detail then
        self.gui:set(id .. '-detail', {text = row.detail, visible = row.detail ~= ''})
    end
end

function Results:showSummary()
    local text, tone = self:badge()
    self.gui:set('checks-summary', {text = text, tone = tone, visible = text ~= ''})
    self.gui:set('checks-empty', {visible = #self.rows == 0})
    self.gui:set('checks-scroll', {visible = #self.rows > 0})
end

-- Draws every row below `top` and returns where the rows end. Rows that do not fit the height of the area in one column take two, and when they still do not fit, every row keeps an equal share of the height, where its detail draws smaller down to three quarters of its size and is cut at the end of its share, so every check always shows.
function Results:draw(area, top)
    top = top or Results.margin
    local available = area.height - top - Results.margin
    local columns = #self.rows > 1 and self:measure(area.width - Results.indent - Results.margin) > available and 2 or 1
    local width = (area.width - Results.margin - Results.gap * (columns - 1)) / columns
    local perColumn = math.ceil(#self.rows / columns)
    local share = nil
    for column = 1, columns do
        local first = (column - 1) * perColumn + 1
        if self:measure(width - Results.indent, first, first + perColumn - 1) > available then
            share = available / perColumn
        end
    end

    local bottom = top
    for column = 1, columns do
        local x = Results.margin + (column - 1) * (width + Results.gap)
        local y = top
        for index = (column - 1) * perColumn + 1, math.min(#self.rows, column * perColumn) do
            y = self:drawRow(self.rows[index], x, y, width, share)
        end
        bottom = math.max(bottom, y)
    end
    return bottom
end

-- The height the rows from `first` to `last` take in one column of the width.
function Results:measure(width, first, last)
    local height = 0
    for index = first or 1, math.min(last or #self.rows, #self.rows) do
        local _, detail = graphics2d.measureText(nil, self.rows[index].detail, {size = Results.detailSize, maxWidth = width})
        height = height + Results.line + detail + Results.spacing
    end
    return height
end

function Results:drawRow(row, x, y, width, share)
    local room = width - Results.indent
    Test.caption(Results.marks[row.state], x, y, {size = 24, color = Results.colors[row.state]})
    graphics2d.drawText(nil, row.name, x + Results.indent - Results.margin, y, {size = 24, color = Test.ink, maxWidth = room, wrap = false, minScale = 0.6, layer = 2})
    local style = {size = Results.detailSize, color = Test.muted, maxWidth = room, layer = 2}
    if not share then
        graphics2d.drawText(nil, row.detail, x + Results.indent - Results.margin, y + Results.line, style)
        local _, height = graphics2d.measureText(nil, row.detail, style)
        return y + Results.line + height + Results.spacing
    end
    local height = share - Results.line - Results.spacing
    style.maxHeight = height
    style.minScale = 0.75
    graphics2d.pushClip({x, y + Results.line, width, height})
    graphics2d.drawText(nil, row.detail, x + Results.indent - Results.margin, y + Results.line, style)
    graphics2d.popClip()
    return y + share
end

return Results
