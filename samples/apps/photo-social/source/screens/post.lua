-- A post with its comments and the field that adds one. A phone shows the post and then the comments in one list over the field, and a wide window shows the picture on a dark stage beside a column with the author, the comments, the buttons and the field.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')
local Widgets = require('widgets')

local Post = haylen.class('Post', Page)

Post.stage = {colors = {panel = '#FF0E0E12'}, metrics = {panelRadius = 0}}

function Post:enter()
    self.post = Social.post(self.params.post)
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Post'),
        ui.column{id = 'body', grow = 1, gap = 0},
        Widgets.friendDialog(self),
    })
    events.on('socialChanged', function(changed)
        if changed == self.post then
            self:showPost()
        end
    end, {owner = self})
end

function Post:layoutChanged(wide)
    self.gui:replaceChildren('body', {wide and self:wideBody() or self:phoneBody()})
    self.list = self.gui:collection('items')
    if not wide then
        Widgets.bindPost(self.list)
    end
    self:showPost()
end

function Post:phoneBody()
    return ui.column{grow = 1, gap = 0,
        ui.collection{id = 'items', grow = 1, gap = 0, padding = {0, 0, Style.space.m, 0},
            types = {post = Widgets.post(false), gallery = Widgets.post(false, true), comment = Widgets.comment()},
            onClick = function(event)
                if event.part == 'comment' or event.part == 'comments' then
                    self.gui:command('draft', 'focus')
                else
                    Widgets.onPostEvent(self, self.list, event)
                end
            end,
            onSelect = function(event)
                if event.part then
                    Widgets.onPostEvent(self, self.list, event)
                end
            end,
        },
        ui.divider{},
        self:commentBar(),
    }
end

function Post:wideBody()
    local space = Style.space
    local post = self.post
    local picture
    if post.pages then
        picture = ui.carousel{id = 'gallery', aspectRatio = 1, align = 'center', grow = 1,
            ui.image{image = post.pages[1], fit = 'cover', align = 'stretch'},
            ui.image{image = post.pages[2], fit = 'cover', align = 'stretch'},
            ui.image{image = post.pages[3], fit = 'cover', align = 'stretch'},
        }
    else
        picture = ui.image{id = 'photo', image = post.photo, fit = 'cover', aspectRatio = 1, align = 'center', grow = 1}
    end
    return ui.row{grow = 1, gap = 0, align = 'stretch',
        ui.panel{grow = 1, padding = space.l, justify = 'center', align = 'stretch', style = Post.stage, picture},
        ui.divider{vertical = true, align = 'stretch'},
        ui.column{width = 520, gap = 0, align = 'stretch',
            ui.row{gap = space.s, padding = {space.m, space.m},
                ui.avatar{image = Social.person(post.author).avatar, size = Style.size.avatar},
                ui.column{gap = 0, grow = 1,
                    ui.button{id = 'author', text = post.authorName, variant = 'link', align = 'start', height = 40, style = Widgets.nameLink, onClick = function()
                        Widgets.openPerson(post.author)
                    end},
                    ui.label{text = post.place, font = 'caption', color = 'textMuted', wrap = false},
                },
            },
            ui.divider{},
            ui.collection{id = 'items', grow = 1, gap = 0, padding = {space.xs, 0}, types = {comment = Widgets.comment()}},
            ui.divider{},
            ui.row{gap = 0, padding = {space.xs, space.xs, 0, space.xs},
                ui.button{id = 'like', icon = Shell.icon('heart'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Like', onClick = function()
                    Social.toggleLike(post)
                end},
                ui.button{id = 'comment', icon = Shell.icon('comment'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Comment', onClick = function()
                    self.gui:command('draft', 'focus')
                end},
                ui.button{id = 'send', icon = Shell.icon('send'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Send to a friend', onClick = function()
                    Widgets.askFriend(self, post)
                end},
                ui.spacer{grow = 1},
                ui.button{id = 'save', icon = Shell.icon('bookmark'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Save', onClick = function()
                    Shell.notify(Social.toggleSave(post) and 'Saved to your collection.' or 'Removed from your collection.')
                end},
            },
            ui.column{gap = space.xxs, padding = {0, space.m, space.s, space.m},
                ui.label{id = 'likes', text = '', font = 'button'},
                ui.label{id = 'age', text = '', font = 'caption', color = 'textMuted'},
            },
            ui.divider{},
            self:commentBar(),
        },
    }
end

-- The field that adds a comment, with the face of the person and the button that posts it.
function Post:commentBar()
    local space = Style.space
    return ui.row{gap = space.s, padding = {space.s, space.m},
        ui.avatar{image = Social.me.avatar, size = Style.size.avatarSmall},
        ui.textField{id = 'draft', placeholder = 'Add a comment for ' .. self.post.authorName, grow = 1, returnKey = 'send', maxLength = 300, style = {metrics = {controlRadius = Style.radius.field}},
            onChange = function(event)
                self.draft = event.value
            end,
            onSubmit = function(event)
                self:send(event.value)
            end},
        ui.button{id = 'postComment', text = 'Post', variant = 'link', onClick = function()
            self:send(self.draft or '')
        end},
    }
end

-- The items of the list: on a phone the post and its comments, and on a wide window the caption as the first comment.
function Post:items()
    local post = self.post
    local items = {}
    if self.wide then
        local author = Social.person(post.author)
        items[1] = {id = post.id .. '-caption', type = 'comment', avatar = author.avatar, markup = post.captionMarkup, meta = post.ageText, likesText = ''}
    else
        items[1] = post
    end
    for _, comment in ipairs(post.comments) do
        items[#items + 1] = comment
    end
    return items
end

function Post:showPost()
    local post = self.post
    self.list:setItems(self:items())
    self.list:reload(self:items()[1].id)
    if self.wide then
        self.gui:set('like', {icon = post.likeIcon, iconColor = post.likeColor})
        self.gui:set('save', {icon = post.saveIcon})
        self.gui:set('likes', {text = post.likesText})
        self.gui:set('age', {text = post.ageText})
    end
end

function Post:send(text)
    if text:match('^%s*$') then
        return
    end
    local comment = Social.comment(self.post, text)
    self.draft = ''
    self.gui:set('draft', {value = ''})
    self.list:scrollTo(comment.id, {align = 'end'})
end

return Post
