-- The tests of one category, each with its code, its title and its description, and a mark on the tests that cannot run on this platform.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local catalog = require('harness.catalog')
local navigation = require('harness.navigation')

local Category = haylen.class('Category', scene.Scene)

-- Takes the test to focus, the one the player comes back from.
function Category:init(category, selected)
    self.category = category
    local last = navigation.lastTest()
    self.selected = selected or (last and last.category == category and last) or category.tests[1]
end

function Category:enter()
    window.setBackLeavesApp(false)
    local rows = {}
    if #self.category.tests == 0 then
        rows[1] = ui.emptyState{title = 'No tests yet', message = 'The manifest of this category lists no tests.'}
    end
    for _, test in ipairs(self.category.tests) do
        local description = test.description
        local reason = catalog.unsupported(test)
        if reason then
            description = string.format('Unsupported on "%s". %s', haylen.platform, description)
        end
        rows[#rows + 1] = ui.row{gap = 24,
            ui.button{id = test.code, text = string.format('%s  %s', test.code, test.title), width = 560, onClick = function()
                self.selected = test
                navigation.openTest(test)
            end},
            ui.label{text = description, color = reason and 'warningText' or 'textMuted', grow = 1},
        }
    end

    self.gui = ui.mount(ui.column{
        padding = {32, 48},
        gap = 24,
        onCancel = navigation.back,
        ui.row{gap = 24, align = 'start',
            ui.button{id = 'back', text = 'Back', onClick = navigation.back},
            ui.pageHeader{title = string.format('%s  %s', self.category.prefix, self.category.title), caption = self.category.description, grow = 1},
        },
        ui.scroll{grow = 1, ui.column{gap = 12, padding = {0, 24, 0, 0}, children = rows}},
    }, {owner = self})
    self.gui:command(self.selected and self.selected.code or 'back', 'focus')
end

-- Coming back from a test keeps the focus on the test that was open, which the UI gives back to the list.
function Category:resume()
    window.setBackLeavesApp(false)
end

return Category
