-- A small app inside the region of the test: a top bar and a bar of tabs that stay while the screens change in the middle, a stack that pushes and pops with a slide, a split of conversations and a chat that sits side by side when it is wide and works as one stack when it is narrow, and a new post that opens as a sheet with bars and a stack of its own. The system back goes through the stacks, then to the first tab, then closes the sheet, and only then back to the list of tests.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Navigation = haylen.class('Navigation', Test)

Navigation.code = [[
tabs = ui.navigationTabs{gui = self.gui, id = 'middle', owner = self, tabs = {
    {id = 'places', root = function() return Places() end},
    {id = 'messages', root = function() return Messages() end}}}
self.stack:push(Place(name))       -- Slides only the middle.
split:showDetail(Chat(person))     -- Beside the list, or over it when narrow.
ui.present(NewPost(), {style = 'sheet'})  -- A new flow with its own bars.]]

Navigation.places = {'Lighthouse', 'Harbor', 'Old mill', 'Pine hill', 'Sea cave', 'Market'}
Navigation.people = {'Ana', 'Bruno', 'Carla', 'Davi'}

-- A place of the list, which opens the next place deeper in the stack.
local Place = haylen.class('Place', ui.Screen)

function Place:init(test, name, depth)
    self.test, self.name, self.depth = test, name, depth
end

function Place:build()
    return ui.column{gap = 12, padding = 16,
        ui.label{text = self.name, font = 'title'},
        ui.label{text = string.format('Level %d of the stack. The bars above and below never move.', self.depth), color = 'textMuted'},
        ui.button{id = 'deeper', text = 'Open the next place', variant = 'primary', autofocus = true, onClick = function()
            local next = Navigation.places[self.depth % #Navigation.places + 1]
            self.stack:push(Place(self.test, next, self.depth + 1))
        end},
        ui.button{id = 'root', text = 'Back to the first place', onClick = function()
            self.stack:popToRoot()
        end},
    }
end

function Place:appeared()
    self.test:showState()
end

-- The root of the first tab: the places, each of which pushes its screen.
local Places = haylen.class('Places', ui.Screen)

function Places:init(test)
    self.test = test
end

function Places:build()
    local items = {}
    for index, name in ipairs(Navigation.places) do
        items[index] = {id = name, text = name, caption = 'Pushes a screen'}
    end
    return ui.column{gap = 8, padding = 16,
        ui.label{text = 'Places', font = 'title'},
        ui.scroll{grow = 1, height = 0, ui.list{id = 'places', items = items, onSelect = function(event)
            self.stack:push(Place(self.test, event.item, 2))
        end}},
    }
end

function Places:appeared()
    self.test:showState()
end

-- A chat with a person, in the detail of the split.
local Chat = haylen.class('Chat', ui.Screen)

function Chat:init(test, person)
    self.test, self.person = test, person
end

function Chat:build()
    return ui.column{gap = 12, padding = 16,
        ui.label{text = 'Chat with ' .. self.person, font = 'title'},
        ui.label{text = 'Hi! The list stays beside this chat on a wide region, and this chat slides over it on a narrow one.', color = 'textMuted'},
        ui.textField{id = 'reply', placeholder = 'Write a reply'},
    }
end

function Chat:appeared()
    self.test:showState()
end

-- What the detail shows while no chat is open.
local Pick = haylen.class('Pick', ui.Screen)

function Pick:build()
    return ui.emptyState{title = 'No chat open', message = 'Pick a person in the list.'}
end

-- The people of the split, each of which opens a chat in its detail.
local People = haylen.class('People', ui.Screen)

function People:init(messages)
    self.messages = messages
end

function People:build()
    local items = {}
    for index, name in ipairs(Navigation.people) do
        items[index] = {id = name, text = name, caption = 'Opens the chat'}
    end
    return ui.column{gap = 8, padding = 16,
        ui.label{text = 'Messages', font = 'title'},
        ui.scroll{grow = 1, height = 0, ui.list{id = 'people', items = items, onSelect = function(event)
            self.messages.split:showDetail(Chat(self.messages.test, event.item))
        end}},
    }
end

-- The root of the second tab: the people on the left and the open chat on the right, or one stack when the region is narrow.
local Messages = haylen.class('Messages', ui.Screen)

function Messages:init(test)
    self.test = test
end

function Messages:build()
    return ui.row{gap = 0, align = 'stretch',
        ui.column{id = 'list', grow = 1, align = 'stretch'},
        ui.divider{id = 'line', vertical = true, align = 'stretch'},
        ui.column{id = 'detail', grow = 2, align = 'stretch'},
    }
end

-- The split lives in the GUI of this screen, so it shows and hides with the tab.
function Messages:appear()
    if not self.split then
        self.split = ui.navigationSplit{primary = {gui = self.gui, id = 'list'}, detail = {gui = self.gui, id = 'detail'}, root = People(self), placeholder = function() return Pick() end, collapsed = self.test.narrow, owner = self}
    end
    self:collapse(self.test.narrow)
end

-- A narrow region hides the detail area, and the chat moves over the list.
function Messages:collapse(narrow)
    self.gui:set('detail', {visible = not narrow})
    self.gui:set('line', {visible = not narrow})
    self.split.collapsed = narrow
end

-- One step of a new post, in the stack of the sheet.
local Step = haylen.class('Step', ui.Screen)

function Step:init(test, number)
    self.test, self.number = test, number
end

function Step:build()
    local lines = {'Pick a picture.', 'Write a caption.', 'Share the post.'}
    return ui.column{gap = 12, padding = 16,
        ui.label{text = string.format('Step %d of 3', self.number), font = 'heading'},
        ui.label{text = lines[self.number], color = 'textMuted'},
    }
end

-- A new post: a flow of its own with a bar that stays while its steps slide under it.
local NewPost = haylen.class('NewPost', ui.Screen)

function NewPost:init(test)
    self.test = test
end

function NewPost:build()
    return ui.column{gap = 0, align = 'stretch',
        ui.row{gap = 12, padding = {8, 16},
            ui.button{id = 'cancel', text = 'Cancel', onClick = function() self.presentation:dismiss() end},
            ui.label{text = 'New post', font = 'heading', grow = 1, textAlign = 'center'},
            ui.button{id = 'next', text = 'Next', variant = 'primary', autofocus = true, onClick = function() self:next() end},
        },
        ui.divider{},
        ui.column{id = 'steps', grow = 1, align = 'stretch'},
    }
end

function NewPost:appear()
    if not self.steps then
        self.steps = ui.navigationStack{gui = self.gui, id = 'steps', root = Step(self.test, 1), owner = self}
    end
end

function NewPost:next()
    if self.steps.size < 3 then
        self.steps:push(Step(self.test, self.steps.size + 1))
        return
    end
    self.presentation:dismiss()
    self.test:status('The new post is shared.')
end

function Navigation:enter()
    self.narrow = false
    self:frame{
        hint = 'Open places to push screens, switch the tabs, open a chat and a new post. The back button, Escape, the east button and the Menu button of a remote go back through them.',
        code = Navigation.code,
        content = {self:app()},
        fixed = true,
        focus = 'tab-places',
    }
    self.gui.sharedFocus = true
    self.tabs = ui.navigationTabs{gui = self.gui, id = 'middle', owner = self, tabs = {
        {id = 'places', root = function() return Places(self) end},
        {id = 'messages', root = function() return Messages(self) end},
    }, onChange = function(event)
        self.gui:set('tab-' .. event.previous, {variant = 'toolbar'})
        self.gui:set('tab-' .. event.selected, {variant = 'primary'})
        self:showState()
    end}
    self:showState()
end

-- The frame of the small app: a top bar with the back button, the title and the new post, the middle and the bar of tabs.
function Navigation:app()
    return ui.card{grow = 1, padding = 0, gap = 0, align = 'stretch',
        ui.row{gap = 12, padding = {8, 12},
            ui.button{id = 'appBack', text = 'Back', onClick = ui.goBack},
            ui.label{id = 'appTitle', text = 'Places', font = 'heading', grow = 1},
            ui.toggle{id = 'narrow', text = 'Narrow', checked = false, onChange = function(event) self:setNarrow(event.checked) end},
            ui.button{id = 'compose', text = 'New post', onClick = function()
                ui.present(NewPost(self), {style = 'sheet', owner = self})
            end},
        },
        ui.divider{},
        ui.column{id = 'middle', grow = 1, align = 'stretch'},
        ui.divider{},
        ui.row{gap = 8, padding = 8,
            ui.button{id = 'tab-places', text = 'Places', variant = 'primary', grow = 1, onClick = function() self.tabs:select('places') end},
            ui.button{id = 'tab-messages', text = 'Messages', variant = 'toolbar', grow = 1, onClick = function() self.tabs:select('messages') end},
        },
    }
end

function Navigation:setNarrow(narrow)
    self.narrow = narrow
    local messages = self.tabs:stack('messages').screens[1]
    if messages.split then
        messages:collapse(narrow)
    end
    self:showState()
end

-- The status says where the system back goes: back in the small app while the navigation can go back, and to the list of tests once it cannot. The first screen appears while the tabs are made, before they are kept.
function Navigation:showState()
    if not self.tabs then
        return
    end
    local stack = self.tabs.current
    self.gui:set('appTitle', {text = self.tabs.selected == 'places' and 'Places' or 'Messages'})
    local screens = stack.size == 1 and '1 screen' or string.format('%d screens', stack.size)
    self:status(string.format('Tab %s, %s in its stack, back goes %s.', self.tabs.selected, screens, ui.canGoBack() and 'back in the app' or 'to the list of tests'))
end

return Navigation
