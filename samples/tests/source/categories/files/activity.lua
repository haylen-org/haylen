-- The latest things a test did, newest first with the time of each, shown in a list of the test page that navigation skips, and a sentence that says how to fill it while it is empty.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Activity = haylen.class('Activity')

Activity.limit = 60

-- Takes the test that owns the page, the id of the list node and the message the list shows while it is empty.
function Activity:init(test, id, empty)
    self.test = test
    self.id = id
    self.empty = empty
    self.items = {}
    self.count = 0
end

-- Returns the node that shows the lines, a list in a scroll area that grows to fill its column, over the empty state until the first line.
function Activity:node()
    return ui.scroll{grow = 1, focusable = false, fill = true,
        ui.column{align = 'stretch', grow = 1,
            ui.emptyState{id = self.id .. '-empty', title = 'Nothing yet', message = self.empty, grow = 1},
            ui.list{id = self.id, items = self.items, visible = false},
        },
    }
end

-- Adds a line, with an optional detail under it.
function Activity:add(text, detail)
    self.count = self.count + 1
    local caption = os.date('%H:%M:%S') .. (detail and '  ' .. detail or '')
    table.insert(self.items, 1, {id = 'line-' .. self.count, text = text, caption = caption})
    self.items[Activity.limit + 1] = nil
    if self.count == 1 then
        self.test:set(self.id .. '-empty', {visible = false})
    end
    self.test:set(self.id, {items = self.items, visible = true})
end

return Activity
