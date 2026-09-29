-- The latest things a test did, newest first with the time of each, shown in a list of the test page that navigation skips.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Activity = haylen.class('Activity')

local kLimit = 60

-- Takes the test that owns the page and the id of the list node.
function Activity:init(test, id)
    self.test = test
    self.id = id
    self.items = {}
    self.count = 0
end

-- Returns the node that shows the lines, a list in a scroll area that grows to fill its column.
function Activity:node()
    return ui.scroll{height = 0, grow = 1, focusable = false, ui.list{id = self.id, items = self.items}}
end

-- Adds a line, with an optional detail under it.
function Activity:add(text, detail)
    self.count = self.count + 1
    local caption = os.date('%H:%M:%S') .. (detail and '  ' .. detail or '')
    table.insert(self.items, 1, {id = 'line-' .. self.count, text = text, caption = caption})
    self.items[kLimit + 1] = nil
    self.test:show(self.id, {items = self.items})
end

return Activity
