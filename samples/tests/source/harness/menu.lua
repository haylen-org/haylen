-- The root screen: the categories by section, a search by code or title over every test, the last test opened and the automatic run of every test.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local catalog = require('harness.catalog')
local navigation = require('harness.navigation')

local Menu = haylen.class('Menu', scene.Scene)

Menu.columns = 4

-- Takes the category to focus, the one the player comes back from.
function Menu:init(selected)
    local last = navigation.lastTest()
    self.selected = selected or (last and last.category) or catalog.categories[1]
end

function Menu:enter()
    window.setBackLeavesApp(true)
    self.document = ui.mount(ui.column{
        padding = {32, 48},
        gap = 24,
        ui.pageHeader{title = haylen.config.name, caption = string.format('%d tests in %d categories on "%s". Pick a category, or search a test by its code or its title.', #catalog.tests, #catalog.categories, haylen.platform)},
        ui.row{id = 'actions', gap = 16, children = self:actions()},
        ui.scroll{grow = 1, ui.column{id = 'body', gap = 16, padding = {0, 24, 0, 0}, children = self:sections()}},
    }, {owner = self})
    self.document:command(self.selected.folder, 'focus')
end

-- Coming back from a category offers the test opened last.
function Menu:resume()
    window.setBackLeavesApp(true)
    local last = navigation.lastTest()
    if last and self.document:has('continue') then
        self.document:set('continue', {text = string.format('Continue with %s', last.code)})
    elseif last then
        self.document:replaceChildren('actions', self:actions())
    end
end

-- The search, the way back to the test opened last and the automatic run of every test.
function Menu:actions()
    local nodes = {
        ui.filterField{id = 'search', placeholder = 'Search by code or title, such as "PHY-004" or "ragdoll"', grow = 1, onChange = function(event)
            self:search(event.value)
        end, onSubmit = function(event)
            local found = catalog.search(event.value)[1]
            if found then
                navigation.openTest(found)
            end
        end},
    }
    local last = navigation.lastTest()
    if last then
        nodes[#nodes + 1] = ui.button{id = 'continue', text = string.format('Continue with %s', last.code), onClick = function()
            navigation.openTest(navigation.lastTest())
        end}
    end
    nodes[#nodes + 1] = ui.button{id = 'runAll', text = 'Run all', onClick = function()
        require('harness.runner').start()
    end}
    return nodes
end

-- The categories under the title of each section, each with its prefix and its number of tests.
function Menu:sections()
    local nodes = {}
    for _, section in ipairs(catalog.sections) do
        nodes[#nodes + 1] = ui.sectionTitle{text = section.title}
        local cells = {}
        for _, category in ipairs(section.categories) do
            cells[#cells + 1] = ui.column{gap = 4,
                ui.button{id = category.folder, text = category.title, align = 'stretch', onClick = function()
                    self.selected = category
                    navigation.openCategory(category)
                end},
                ui.label{text = string.format('%s, %d tests', category.prefix, #category.tests), font = 'caption', color = 'textMuted'},
            }
        end
        nodes[#nodes + 1] = ui.grid{columns = Menu.columns, gap = 16, children = cells}
    end
    return nodes
end

-- Shows the tests that match the query in place of the categories, or the categories again once the query is empty.
function Menu:search(query)
    if query:match('^%s*$') then
        self.document:replaceChildren('body', self:sections())
        return
    end
    local rows = {}
    for _, test in ipairs(catalog.search(query)) do
        rows[#rows + 1] = ui.row{gap = 24,
            ui.button{id = 'found-' .. test.code, text = string.format('%s  %s', test.code, test.title), width = 560, onClick = function()
                navigation.openTest(test)
            end},
            ui.label{text = test.description, color = 'textMuted', grow = 1},
        }
    end
    if #rows == 0 then
        rows[1] = ui.emptyState{title = 'No test matches', message = string.format('No code or title contains "%s".', query)}
    end
    self.document:replaceChildren('body', rows)
end

return Menu
