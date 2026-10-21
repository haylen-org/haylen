-- A profile: the face, the counts of posts, followers and following, the name and the bio, the buttons that follow and message, or edit the profile and start a post on the person's own, and the grid of pictures, with the saved posts in a second section of the person's own. The profile of the tab is the person's own, and the others open over it.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Chats = require('chats')
local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')
local Widgets = require('widgets')

local Profile = haylen.class('Profile', Page)

Profile.sections = {{id = 'posts', text = 'Posts'}, {id = 'saved', text = 'Saved'}}

function Profile:enter()
    local person = Social.person(self.params.person or 'me')
    self.person = person
    self.section = 'posts'
    local space = Style.space
    local list = ui.collection{id = 'grid', grow = 1, layout = 'grid', lanes = 3, cellAspect = 1, gap = space.xs, padding = {0, space.s, space.xl, space.s}, rememberFocus = true,
        types = {head = {template = self:headTemplate(), span = 'full', interactive = false, estimatedSize = 560}, tile = Widgets.tile()},
        onClick = function(event)
            self:headAction(event.part)
        end,
        onChange = function(event)
            if event.part == 'section' then
                self.section = event.value
                self:showGrid()
            end
        end,
        onSelect = function(event)
            if not event.part then
                Shell.open(require('screens.post'), {post = event.item})
            end
        end,
    }
    local content = ui.column{grow = 1, gap = 0,
        person.me and ui.column{id = 'topBar', gap = 0, ui.row{height = Style.size.header, padding = {0, space.m}, ui.label{text = person.handle, font = 'heading', grow = 1, wrap = false}}, ui.divider{}} or Shell.header(person.handle),
        Shell.centered(980, ui.column{gap = 0, list}, 1),
        self:editDialog(),
    }
    self:mountGui(person.me and Shell.frame('profile', content) or ui.column{gap = 0, onCancel = Shell.back, content})
    if person.me then
        Shell.followUnread(self)
    end
    self.list = self.gui:collection('grid')
    events.on('socialChanged', function()
        self:showGrid()
    end, {owner = self})
    self:showGrid()
    -- The head is measured once it draws, and the grid keeps showing it from the top.
    self.list:scrollTo('head', {align = 'start', animated = false})
end

function Profile:layoutChanged(wide)
    if self.person.me and self.gui:has('topBar') then
        self.gui:set('topBar', {visible = not wide})
    end
end

-- The head of the profile, the first cell of the grid, across all three columns: the face inside the ring of the app, the name and the bio in the middle, and the counts on a soft card.
function Profile:headTemplate()
    local space = Style.space
    local function count(part, text)
        return ui.column{gap = 0, grow = 1,
            ui.label{part = part, bind = {text = part}, text = '0', font = 'heading', textAlign = 'center', align = 'stretch'},
            ui.label{text = text, font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'},
        }
    end
    return ui.column{gap = space.m, padding = {space.l, space.xxs, space.l, space.xxs},
        ui.stack{width = 156, height = 156, align = 'center',
            ui.image{image = 'images/ring.svg', align = 'stretch', accessibilityHidden = true},
            ui.avatar{part = 'avatar', bind = {image = 'avatar'}, size = 132, align = 'center'},
        },
        ui.column{gap = space.xxs, align = 'stretch',
            ui.label{part = 'name', bind = {text = 'name'}, text = '', font = 'heading', textAlign = 'center', align = 'stretch'},
            ui.label{part = 'bio', bind = {text = 'bio'}, text = '', color = 'textMuted', textAlign = 'center', align = 'stretch'},
        },
        ui.panel{padding = {space.s, space.xs}, align = 'stretch', style = {colors = {panel = Style.color.mist}, metrics = {panelRadius = Style.radius.card}},
            ui.row{gap = space.xs, align = 'stretch', count('posts', 'Posts'), count('followers', 'Followers'), count('following', 'Following')},
        },
        ui.row{gap = space.s,
            ui.button{part = 'primary', bind = {text = 'primaryText', variant = 'primaryVariant'}, text = '', grow = 1},
            ui.button{part = 'secondary', bind = {text = 'secondaryText'}, text = '', grow = 1},
        },
        ui.segmentedControl{part = 'section', bind = {visible = 'mine'}, items = Profile.sections, selected = 'posts', visible = false},
    }
end

function Profile:headItem()
    local person = self.person
    local mine = person.me == true
    return {
        id = 'head',
        type = 'head',
        avatar = person.avatar,
        name = person.name,
        bio = person.bio,
        posts = Social.thousands(#person.posts),
        followers = Social.thousands(person.followers),
        following = Social.thousands(person.following),
        primaryText = mine and 'Edit profile' or (person.followed and 'Following' or 'Follow'),
        primaryVariant = (mine or person.followed) and 'default' or 'primary',
        secondaryText = mine and 'New post' or 'Message',
        mine = mine,
    }
end

function Profile:showGrid()
    local posts = self.section == 'saved' and Social.savedPosts() or self.person.posts
    local items = Widgets.tiles(posts)
    table.insert(items, 1, self:headItem())
    self.list:setItems(items)
    self.list:reload('head')
end

function Profile:headAction(part)
    local person = self.person
    if part == 'primary' then
        if person.me then
            self.gui:set('editName', {value = person.name})
            self.gui:set('editBio', {value = person.bio})
            self.edit = {name = person.name, bio = person.bio}
            self.gui:set('editDialog', {open = true})
        else
            Social.toggleFollow(person)
        end
    elseif part == 'secondary' then
        if person.me then
            Shell.showTab('create')
        else
            Shell.open(require('screens.chat'), {conversation = Chats.with(person).id})
        end
    end
end

-- The dialog that edits the name and the bio of the person's own profile.
function Profile:editDialog()
    if not self.person.me then
        return ui.spacer{}
    end
    return ui.dialog{id = 'editDialog', title = 'Edit profile', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'save', text = 'Save', variant = 'primary'}},
        onAnswer = function(event)
            if event.button == 'save' then
                local name = self.edit.name:match('^%s*(.-)%s*$')
                self.person.name = name ~= '' and name or self.person.name
                self.person.bio = self.edit.bio
                self:showGrid()
                Shell.notify('Your profile is up to date.')
            end
        end,
        ui.formField{label = 'Name', ui.textField{id = 'editName', maxLength = 40, autocapitalize = 'words', returnKey = 'next', onChange = function(event)
            self.edit.name = event.value
        end}},
        ui.formField{label = 'Bio', ui.textField{id = 'editBio', maxLength = 80, returnKey = 'done', onChange = function(event)
            self.edit.bio = event.value
        end}},
    }
end

return Profile
