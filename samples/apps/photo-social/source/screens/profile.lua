-- A profile: the face, the counts of posts, followers and following, the name and the bio, the buttons that follow and message, or edit the profile and start a post on the person's own, and the grid of pictures, with the account and the saved posts in a second section of the person's own. The profile of the tab is the person's own, and the others open over it.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Account = require('account')
local Page = require('page')
local Routes = require('routes')
local SignIn = require('screens.sign-in')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')
local Widgets = require('widgets')

local Profile = haylen.class('Profile', Page)

-- The name and the bio of the person's own profile, which asks before it drops changes nobody saved.
local EditProfile = haylen.class('EditProfile', ui.Screen)

function EditProfile:init(person)
    self.person = person
    self.name, self.bio = person.name, person.bio
end

function EditProfile:build()
    local space = Style.space
    return ui.column{gap = space.m, padding = {space.xs, space.l, space.l, space.l},
        ui.row{gap = space.s,
            ui.button{id = 'cancel', text = 'Cancel', variant = 'link', onClick = function() ui.goBack() end},
            ui.label{text = 'Edit profile', font = 'heading', grow = 1, textAlign = 'center'},
            ui.button{id = 'save', text = 'Save', variant = 'primary', onClick = function() self.presentation:dismiss({result = {name = self.name, bio = self.bio}}) end},
        },
        ui.formField{label = 'Name', ui.textField{id = 'editName', value = self.name, maxLength = 40, autocapitalize = 'words', returnKey = 'next', onChange = function(event)
            self.name = event.value
        end}},
        ui.formField{label = 'Bio', ui.textArea{id = 'editBio', value = self.bio, maxLength = 80, rows = 2, onChange = function(event)
            self.bio = event.value
        end}},
    }
end

-- Cancel, Escape, the back button, a tap beside the sheet and a drag down ask first while the profile has changes nobody saved.
function EditProfile:back()
    if self.name == self.person.name and self.bio == self.person.bio then
        return false
    end
    ui.presentAlert{title = 'Discard your changes?', message = 'The name and the bio you typed are not saved.', owner = self,
        actions = {{id = 'keep', text = 'Keep editing', role = 'cancel'}, {id = 'discard', text = 'Discard', role = 'destructive'}},
        onDismiss = function(event)
            if event.result == 'discard' then
                self.presentation:dismiss()
            end
        end}
    return true
end

Profile.sections = {{id = 'posts', text = 'Posts'}, {id = 'saved', text = 'Saved'}}

function Profile:tree()
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
                Shell.open(Routes.post(Social.post(event.item)))
            end
        end,
    }
    local content = ui.column{grow = 1, gap = 0,
        self.params.person == nil and ui.column{id = 'topBar', gap = 0, ui.row{height = Style.size.header, padding = {0, space.m}, ui.label{text = person.handle, font = 'heading', grow = 1, wrap = false}}, ui.divider{}} or Shell.header(person.handle),
        Shell.centered(980, ui.column{gap = 0, list}, 1),
    }
    return content
end

function Profile:start()
    self.list = self.gui:collection('grid')
    events.on('socialChanged', function()
        self:showGrid()
    end, {owner = self})
    events.on('accountChanged', function()
        self:showGrid()
    end, {owner = self})
    self:showGrid()
    -- The head is measured once it draws, and the grid keeps showing it from the top.
    self.list:scrollTo('head', {align = 'start', animated = false})
end

function Profile:layoutChanged(wide)
    if self.gui:has('topBar') then
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
        ui.row{part = 'accountRow', bind = {visible = 'mine'}, visible = false, gap = space.s, align = 'stretch',
            ui.label{part = 'accountText', bind = {text = 'accountText'}, text = '', color = 'textMuted', grow = 1},
            ui.button{part = 'account', bind = {text = 'accountAction'}, text = '', variant = 'link'},
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
        accountText = Account.email and ('Signed in as ' .. Account.email) or 'Not signed in',
        accountAction = Account.email and 'Sign out' or 'Sign in',
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
            self:openEdit()
        else
            Widgets.toggleFollow(person)
        end
    elseif part == 'secondary' then
        if person.me then
            Shell.showTab('create')
        else
            Shell.router:open(Routes.chat(person.id))
        end
    elseif part == 'account' then
        if Account.email then
            Account.signOut()
            Shell.notify('You signed out.')
        else
            self:openSignIn()
        end
    end
end

-- Edits the name and the bio of the person's own profile in a sheet, which hands back what the person saved.
function Profile:openEdit()
    self.editSheet = ui.present(EditProfile(self.person), {owner = self, onDismiss = function(event)
        if not event.result then
            return
        end
        local name = event.result.name:match('^%s*(.-)%s*$')
        self.person.name = name ~= '' and name or self.person.name
        self.person.bio = event.result.bio
        self:showGrid()
        Shell.notify('Your profile is up to date.')
    end})
end

-- Signs the person in through a sheet that fits its fields, which hands back the email of the account.
function Profile:openSignIn()
    self.signInSheet = ui.present(SignIn(), {detents = {'fit'}, owner = self, onDismiss = function(event)
        if event.result then
            Shell.notify('Signed in as ' .. event.result .. '.')
        end
    end})
end

return Profile
