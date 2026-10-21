-- The explore tab: a grid of the pictures of everyone, and the people whose name or handle matches the typed text.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')
local Widgets = require('widgets')

local Explore = haylen.class('Explore', Page)

function Explore:enter()
    local space = Style.space
    self.query = ''
    self:mountGui(Shell.frame('explore', ui.column{grow = 1, gap = 0,
        Shell.centered(980, ui.column{gap = 0,
            ui.row{padding = {space.s, space.m}, ui.filterField{id = 'query', placeholder = 'Search people', grow = 1, returnKey = 'search', style = {metrics = {controlRadius = Style.radius.field}}, onChange = function(event)
                self.query = string.lower(event.value)
                self:refresh()
            end}},
        }),
        Shell.centered(980, ui.column{gap = 0,
            ui.collection{id = 'grid', grow = 1, layout = 'grid', lanes = 3, cellAspect = 1, gap = space.xxs, padding = {0, 0, space.xl, 0}, rememberFocus = true, types = {tile = Widgets.tile()},
                onSelect = function(event)
                    Shell.open(require('screens.post'), {post = event.item})
                end,
            },
            ui.collection{id = 'people', visible = false, grow = 1, gap = 0, padding = {0, 0, space.xl, 0}, rememberFocus = true, types = {person = Widgets.person()},
                onClick = function(event)
                    local person = Social.person(event.cell.item)
                    Social.toggleFollow(person)
                    Widgets.describePerson(person)
                    self.gui:collection('people'):reload(person.id)
                end,
                onSelect = function(event)
                    Widgets.openPerson(event.item)
                end,
                ui.emptyState{image = Shell.icon('search'), imageSize = 64, title = 'Nobody by that name', message = 'Try another name or handle.'},
            },
        }, 1),
    }))
    Shell.followUnread(self)
    local posts = {}
    for _, post in ipairs(Social.posts) do
        if post.author ~= 'me' then
            posts[#posts + 1] = post
        end
    end
    self.gui:collection('grid'):setItems(Widgets.tiles(posts))
    if self.params.focus then
        self.gui:command('query', 'focus')
    end
end

function Explore:refresh()
    local searching = self.query ~= ''
    self.gui:set('grid', {visible = not searching})
    self.gui:set('people', {visible = searching})
    if not searching then
        return
    end
    local people = {}
    for _, person in ipairs(Social.people) do
        if string.find(string.lower(person.name .. ' ' .. person.handle), self.query, 1, true) then
            people[#people + 1] = Widgets.describePerson(person)
        end
    end
    self.gui:collection('people'):setItems(people)
end

return Explore
