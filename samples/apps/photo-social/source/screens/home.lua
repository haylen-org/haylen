-- The home tab: the row of stories and the feed of posts from the people the person follows. A phone shows the pictures from edge to edge under the wordmark, and a wide window shows the posts as cards in a column, with suggestions of people to follow beside it when there is room.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')
local Widgets = require('widgets')

local Home = haylen.class('Home', Page)

function Home:enter()
    local space = Style.space
    self:mountGui(Shell.frame('home', ui.column{grow = 1, gap = 0,
        ui.column{id = 'topBar', gap = 0,
            ui.row{height = Style.size.header, padding = {0, space.xs, 0, space.m}, Shell.wordmark(44), ui.spacer{grow = 1},
                ui.button{id = 'explore', icon = Shell.icon('search'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Explore', tooltip = 'Explore', onClick = function()
                    Shell.showTab('explore', {focus = true})
                end},
            },
            ui.divider{},
        },
        ui.row{id = 'body', grow = 1, gap = 0, align = 'stretch'},
        Widgets.friendDialog(self),
    }))
    Shell.followUnread(self)
    events.on('socialChanged', function(changed)
        if self.feed and Social.post(changed.id) == changed then
            self.feed:setItems(Social.feed())
            if self.feed:indexOf(changed.id) then
                self.feed:reload(changed.id)
            end
        end
    end, {owner = self})
end

function Home:layoutChanged(wide, roomy)
    local state = self.feed and self.feed:saveState()
    self.gui:set('topBar', {visible = not wide})
    self.gui:replaceChildren('body', {self:body(wide, roomy)})
    self.stories = self.gui:collection('stories')
    self.feed = self.gui:collection('feed')
    self.stories:setItems(self:storyItems())
    self.feed:setItems(Social.feed())
    Widgets.bindPost(self.feed)
    if state then
        self.feed:restoreState(state)
    end
    if roomy then
        self.gui:collection('suggested'):setItems(self:suggestions())
    end
end

function Home:body(wide, roomy)
    local space = Style.space
    local column = ui.column{gap = 0, grow = 1,
        ui.collection{id = 'stories', axis = 'horizontal', scrollbar = false, gap = space.xs, padding = {space.s, wide and space.xs or space.s}, rememberFocus = true, types = {story = Widgets.story()},
            onSelect = function(event)
                self:openStory(event.item)
            end,
        },
        ui.divider{},
        ui.collection{id = 'feed', grow = 1, gap = wide and space.l or 0, padding = wide and {space.l, 0, space.xl, 0} or {0, 0, space.l, 0}, scrollbar = wide, rememberFocus = true,
            types = {post = Widgets.post(wide), gallery = Widgets.post(wide, true)},
            onClick = function(event)
                Widgets.onPostEvent(self, self.feed, event)
            end,
            onSelect = function(event)
                if event.part then
                    Widgets.onPostEvent(self, self.feed, event)
                end
            end,
        },
    }
    if not wide then
        return column
    end
    if not roomy then
        return Shell.centered(Style.size.column, column, 1)
    end
    column.maxWidth = Style.size.column
    column.grow = 100
    return ui.row{grow = 1, gap = space.xxl, padding = {0, space.l}, align = 'stretch',
        ui.spacer{grow = 1},
        column,
        self:side(),
        ui.spacer{grow = 1},
    }
end

-- The column beside the feed on roomy windows: the person who uses the app and people to follow.
function Home:side()
    local space = Style.space
    local me = Social.me
    return ui.column{width = Style.size.side, gap = space.m, padding = {space.xl, 0, 0, 0}, align = 'start',
        ui.row{gap = space.s,
            ui.avatar{image = me.avatar, size = 64},
            ui.column{gap = 0, grow = 1, ui.label{text = me.handle, font = 'button', wrap = false}, ui.label{text = me.name, font = 'caption', color = 'textMuted', wrap = false}},
        },
        ui.label{text = 'Suggested for you', font = 'button', color = 'textMuted', margin = {space.s, 0, 0, 0}},
        ui.collection{id = 'suggested', height = 80 * 5, gap = 0, scrollbar = false, types = {person = Widgets.person()},
            onClick = function(event)
                local person = Social.person(event.cell.item)
                Social.toggleFollow(person)
                Widgets.describePerson(person)
                self.gui:collection('suggested'):reload(person.id)
            end,
            onSelect = function(event)
                Widgets.openPerson(event.item)
            end,
        },
    }
end

function Home:suggestions()
    local list = {}
    for _, person in ipairs(Social.people) do
        if not person.followed and #list < 5 then
            list[#list + 1] = Widgets.describePerson(person)
        end
    end
    return list
end

-- The stories: the person's own first, which starts a new post, and then the others, the unseen ones inside the colored ring.
function Home:storyItems()
    local me = Social.me
    local items = {{id = 'mine', type = 'story', name = 'Your story', avatar = me.avatar, ring = 'images/ring-seen.svg', mine = true}}
    for _, story in ipairs(Social.stories) do
        story.ring = story.seen and 'images/ring-seen.svg' or 'images/ring.svg'
        items[#items + 1] = story
    end
    return items
end

function Home:openStory(id)
    if id == 'mine' then
        Shell.showTab('create')
        return
    end
    for index, story in ipairs(Social.stories) do
        if story.id == id then
            Shell.open(require('screens.story'), {index = index}, Shell.riseTransition)
            return
        end
    end
end

-- Shows the stories seen while the viewer was open inside the gray ring.
function Home:resume()
    if self.stories then
        self.stories:setItems(self:storyItems())
        self.stories:reload(self:storyIds())
    end
end

function Home:storyIds()
    local ids = {}
    for _, story in ipairs(Social.stories) do
        ids[#ids + 1] = story.id
    end
    return ids
end

return Home
