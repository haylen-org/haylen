-- Routes of links in a small app inside the region of the test: the links of the scheme of the native demo plugin open a place in the tab of places with the places under it, a chat as the detail of the split of messages and a new post as a sheet. A link that no route matches leaves the app as it is, the buttons go to the same routes from code, and the state of the navigation reads back as the locations the router keeps for the next start.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local demo = require('native-demo')

local Routes = haylen.class('Routes', Test)

Routes.code = [[
router = ui.router{tabs = tabs, schemes = {'haylendemo'}, restore = true, routes = {
    {name = 'place', path = 'place/{name}', tab = 'places', parent = 'places', screen = function(p) return Place(p.name) end},
    {name = 'chat', path = 'chat/{person}', tab = 'messages', detail = true, screen = function(p) return Chat(p.person) end},
    {name = 'post', path = 'post', present = {detents = {'medium', 'large'}}, screen = function() return NewPost() end}}}
router:open('haylendemo://place/harbor?zoom=3')  -- Or a link from outside.]]

Routes.places = {'harbor', 'lighthouse', 'mill'}
Routes.people = {'ana', 'bruno'}

-- How to open a link of the scheme on each platform, from a terminal of the computer or from the page.
Routes.ways = {
    ios = 'xcrun simctl openurl booted %s://place/mill',
    tvos = 'xcrun simctl openurl booted %s://place/mill',
    macos = 'open %s://place/mill',
    android = 'adb shell am start -a android.intent.action.VIEW -d %s://place/mill',
    windows = 'start "" %s://place/mill',
    linux = 'xdg-open %s://place/mill',
}

-- A screen that shows its title and a line about where it lies.
local Page = haylen.class('Page', ui.Screen)

function Page:init(title, text)
    self.title, self.text = title, text
end

function Page:build()
    return ui.column{gap = 12, padding = 16,
        ui.label{text = self.title, font = 'heading'},
        ui.label{text = self.text, color = 'textMuted'},
    }
end

-- The root of the tab of messages: the people beside the open chat, whose split the route of a chat shows its detail in.
local Messages = haylen.class('Messages', ui.Screen)

function Messages:build()
    return ui.row{gap = 0, align = 'stretch',
        ui.column{id = 'list', grow = 1, align = 'stretch'},
        ui.divider{vertical = true, align = 'stretch'},
        ui.column{id = 'detail', grow = 2, align = 'stretch'},
    }
end

function Messages:appear()
    if not self.split then
        self.split = ui.navigationSplit{primary = {gui = self.gui, id = 'list'}, detail = {gui = self.gui, id = 'detail'}, root = Page('People', 'Open a chat with a link or a button.'), placeholder = function() return Page('No chat', 'A chat shows here.') end, owner = self}
    end
end

function Routes:enter()
    local scheme = demo.config().urlScheme
    local way = Routes.ways[haylen.platform]
    self:frame{
        hint = way and 'Tap the links, or run this to open one from outside: ' .. string.format(way, scheme) or 'Tap the links to go to their routes from code.',
        code = Routes.code,
        content = {self:app(scheme)},
        fixed = true,
        focus = 'link-place',
    }
    self.gui.sharedFocus = true
    self.tabs = ui.navigationTabs{gui = self.gui, id = 'middle', owner = self, tabs = {
        {id = 'places', root = function() return Page('Places', 'The places of the island.') end},
        {id = 'messages', root = function() return Messages() end},
    }}
    self.router = ui.router{tabs = self.tabs, schemes = {scheme}, owner = self, routes = {
        {name = 'places', path = 'places', tab = 'places'},
        {name = 'place', path = 'place/{name}', query = {zoom = 'integer'}, parent = 'places', screen = function(p)
            return Page('Place: ' .. p.name, p.zoom and string.format('Zoom %d, from the query of the link.', p.zoom) or 'A place that a link opened.')
        end},
        {name = 'chat', path = 'chat/{person}', tab = 'messages', detail = true, screen = function(p) return Page('Chat with ' .. p.person, 'The detail of the split of messages.') end},
        {name = 'post', path = 'post', present = {detents = {'medium', 'large'}}, screen = function() return Page('New post', 'A sheet that a link presented.') end},
    }, onUnmatched = function(event)
        self:report('No route matches "' .. event.url .. '", so the app stays as it is.')
    end}
    self:report('Open a link.')
end

function Routes:report(text)
    self:set('status', {text = text})
end

-- Opens a link like one from outside, and shows where the router went and the state it keeps.
function Routes:follow(link)
    if not self.router:match(link) then
        self:report('No route matches "' .. link .. '", so the app stays as it is.')
        return
    end
    self.router:open(link)
    local state = self.router:save()
    self:report(string.format('Opened "%s". The tab "%s" holds %s.', link, state.tab, table.concat(state.stacks[state.tab] or {}, ', ')))
end

function Routes:app(scheme)
    local links = {gap = 8, wrap = true}
    for _, entry in ipairs({{'place', 'place/harbor?zoom=3'}, {'mill', 'place/mill'}, {'chat', 'chat/ana'}, {'post', 'post'}, {'lost', 'nowhere/7'}}) do
        local link = scheme .. '://' .. entry[2]
        links[#links + 1] = ui.button{id = 'link-' .. entry[1], text = link, variant = 'link', onClick = function() self:follow(link) end}
    end
    return ui.card{grow = 1, padding = 0, gap = 0, align = 'stretch',
        ui.row{gap = 12, padding = {8, 12},
            ui.button{id = 'appBack', text = 'Back', onClick = ui.goBack},
            ui.row(links),
        },
        ui.divider{},
        ui.column{id = 'middle', grow = 1, align = 'stretch'},
        ui.divider{},
        ui.row{gap = 8, padding = 8,
            ui.button{id = 'tab-places', text = 'Places', grow = 1, onClick = function() self.router:push('places') end},
            ui.button{id = 'tab-messages', text = 'Messages', grow = 1, onClick = function() self.tabs:select('messages') end},
        },
    }
end

return Routes
