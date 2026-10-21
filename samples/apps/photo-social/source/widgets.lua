-- The building blocks the screens share: the templates of posts, comments, people, stories and the tiles of a grid, what the buttons of a post do, and the dialog that sends a post to a friend.
local tween = require('haylen.tween')
local math2d = require('haylen.math')
local ui = require('haylen.ui')

local Chats = require('chats')
local Photos = require('photos')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')

local Widgets = {}

Widgets.placeholder = Photos.look('lake-sunrise', 'full')
-- The units on each side of the documents of the faces, which a picture button scales to the size of a face.
Widgets.faceSize = 120
Widgets.postMenu = {
    {id = 'profile', text = 'Go to the profile', image = Shell.icon('home')},
    {id = 'send', text = 'Send to a friend', image = Shell.icon('send')},
    {id = 'save', text = 'Save or unsave', image = Shell.icon('bookmark')},
}

-- The template of a post: the author, the picture or the pages of pictures, the buttons, the likes, the caption and the way to the comments. A post on a wide window sits in a card with rounded pictures, and on a phone the picture runs from edge to edge.
function Widgets.post(wide, gallery)
    local space = Style.space
    local gutter = wide and space.l or space.m
    local radius = wide and Style.radius.photo or 0
    local picture
    if gallery then
        picture = ui.carousel{part = 'gallery', aspectRatio = 1, align = 'stretch', arrows = wide, accessibilityLabel = 'Pictures of the post',
            ui.image{part = 'page1', bind = {image = 'page1'}, image = Widgets.placeholder, fit = 'cover', align = 'stretch', radius = radius},
            ui.image{part = 'page2', bind = {image = 'page2'}, image = Widgets.placeholder, fit = 'cover', align = 'stretch', radius = radius},
            ui.image{part = 'page3', bind = {image = 'page3'}, image = Widgets.placeholder, fit = 'cover', align = 'stretch', radius = radius},
        }
    else
        picture = ui.stack{align = 'stretch',
            ui.image{part = 'photo', bind = {image = 'photo'}, image = Widgets.placeholder, fit = 'cover', aspectRatio = 1, align = 'stretch', radius = radius, accessibilityLabel = 'Picture of the post'},
            ui.icon{part = 'pop', image = Shell.icon('heart-filled'), size = 168, color = 'onAccent', align = 'center', accessibilityHidden = true},
        }
    end
    local body = ui.column{gap = 0, padding = wide and {0, 0, space.m, 0} or {0, 0, space.xl, 0},
        ui.row{gap = space.s, padding = {space.s, gutter - space.xs, space.s, gutter},
            ui.imageButton{part = 'face', bind = {image = 'avatar'}, image = Social.me.avatar, scale = Style.size.avatar / Widgets.faceSize, accessibilityLabel = 'Profile of the author'},
            ui.column{gap = 0, grow = 1,
                ui.button{part = 'author', bind = {text = 'authorName'}, text = '', variant = 'link', align = 'start', height = 40, style = Widgets.nameLink},
                ui.label{part = 'place', bind = {text = 'place'}, font = 'caption', color = 'textMuted', wrap = false},
            },
            ui.menuButton{part = 'more', icon = Shell.icon('more'), iconColor = 'text', variant = 'icon', items = Widgets.postMenu, accessibilityLabel = 'More'},
        },
        wide and ui.column{padding = {0, gutter}, picture} or picture,
        ui.row{gap = 0, padding = {space.xs, gutter - space.s, 0, gutter - space.s},
            ui.button{part = 'like', bind = {icon = 'likeIcon', iconColor = 'likeColor'}, icon = Shell.icon('heart'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Like'},
            ui.button{part = 'comment', icon = Shell.icon('comment'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Comment'},
            ui.button{part = 'send', icon = Shell.icon('send'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Send to a friend'},
            ui.spacer{grow = 1},
            ui.button{part = 'save', bind = {icon = 'saveIcon'}, icon = Shell.icon('bookmark'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Save'},
        },
        ui.column{gap = space.xxs, padding = {0, gutter},
            ui.label{part = 'likes', bind = {text = 'likesText'}, font = 'button'},
            ui.richText{part = 'caption', bind = {text = 'captionMarkup'}, text = ''},
            ui.button{part = 'comments', bind = {text = 'commentsText'}, text = '', variant = 'link', align = 'start', style = Widgets.quietLink},
            ui.label{part = 'age', bind = {text = 'ageText'}, font = 'caption', color = 'textMuted'},
        },
    }
    local template = wide and ui.card{padding = {space.s, 0, 0, 0}, body} or body
    return {template = template, estimatedSize = wide and 980 or 860}
end

-- A link in the muted color of the theme, and the name of a person as a link in the color of the text, both without the padding of buttons.
Widgets.quietLink = {colors = {accentText = 'textMuted', accentHover = 'text'}, metrics = {controlPaddingX = 0}}
Widgets.nameLink = {colors = {accentText = 'text', accentHover = 'text'}, metrics = {controlPaddingX = 0}}

-- Hides the heart that pops over the picture of a post, which `Widgets.popHeart` shows for a moment.
function Widgets.bindPost(list)
    list:setBinder('post', function(cell)
        cell:transform('pop').opacity = 0
    end)
end

function Widgets.popHeart(page, list, post)
    local cell = list:cellOf(post.id)
    if not cell or post.type ~= 'post' then
        return
    end
    local pop = cell:transform('pop')
    tween.fromTo(pop, 0.7, {opacity = 1, scale = math2d.vec2(0.6, 0.6)}, {opacity = 0, scale = math2d.vec2(1.15, 1.15)}, {ease = 'cubicOut', owner = page, reducedMotion = 'skip'})
end

-- What a screen does for the events of a list of posts: likes, saves, comments, menus and sending.
function Widgets.onPostEvent(page, list, event)
    local post = Social.post(event.cell.item)
    local part = event.part
    if part == 'like' then
        if Social.toggleLike(post) then
            Widgets.popHeart(page, list, post)
        end
        list:reload(post.id)
    elseif part == 'save' then
        Shell.notify(Social.toggleSave(post) and 'Saved to your collection.' or 'Removed from your collection.')
        list:reload(post.id)
    elseif part == 'comment' or part == 'comments' then
        Shell.open(require('screens.post'), {post = post.id})
    elseif part == 'send' then
        Widgets.askFriend(page, post)
    elseif part == 'author' or part == 'face' then
        Widgets.openPerson(post.author)
    elseif part == 'more' then
        if event.item == 'profile' then
            Widgets.openPerson(post.author)
        elseif event.item == 'send' then
            Widgets.askFriend(page, post)
        else
            Shell.notify(Social.toggleSave(post) and 'Saved to your collection.' or 'Removed from your collection.')
            list:reload(post.id)
        end
    end
end

function Widgets.openPerson(id)
    Shell.open(require('screens.profile'), {person = id})
end

-- The dialog that sends a post to one of the conversations, which every screen of posts mounts.
function Widgets.friendDialog(page)
    local items = {}
    for _, conversation in ipairs(Chats.list) do
        items[#items + 1] = {id = conversation.id, text = conversation.name, caption = conversation.handle, image = conversation.avatar, iconColor = false}
    end
    page.friend = items[1].id
    return ui.dialog{id = 'friendDialog', title = 'Send to a friend', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'send', text = 'Send', variant = 'primary'}},
        onAnswer = function(event)
            if event.button == 'send' then
                local conversation = Chats.find(page.friend)
                Chats.send(conversation, 'Look at this post', page.sharing)
                Shell.notify('Sent to ' .. conversation.name .. '.')
            end
        end,
        ui.list{id = 'friendList', items = items, selected = items[1].id, height = 72 * 4, onSelect = function(event)
            page.friend = event.item
        end},
    }
end

function Widgets.askFriend(page, post)
    page.sharing = post
    page.gui:set('friendDialog', {open = true})
end

-- The template of a comment: the face, the name with the text, how long ago and the likes.
function Widgets.comment()
    local space = Style.space
    return {template = ui.row{gap = space.s, padding = {space.s, space.m}, alignItems = 'start',
        ui.avatar{part = 'avatar', bind = {image = 'avatar'}, size = Style.size.avatarSmall},
        ui.column{gap = space.xxs, grow = 1,
            ui.richText{part = 'text', bind = {text = 'markup'}, text = ''},
            ui.row{gap = space.m,
                ui.label{part = 'meta', bind = {text = 'meta'}, font = 'caption', color = 'textMuted'},
                ui.label{part = 'likes', bind = {text = 'likesText'}, font = 'caption', color = 'textMuted'},
            },
        },
    }, interactive = false, estimatedSize = 92}
end

-- The template of a person in a list: the face, the handle, the name and the button that follows.
function Widgets.person()
    local space = Style.space
    return {template = ui.row{gap = space.s, padding = {space.xs, space.m},
        ui.avatar{part = 'avatar', bind = {image = 'avatar'}, size = Style.size.avatar},
        ui.column{gap = 0, grow = 1,
            ui.label{part = 'handle', bind = {text = 'handle'}, font = 'button', wrap = false},
            ui.label{part = 'name', bind = {text = 'name'}, font = 'caption', color = 'textMuted', wrap = false},
        },
        ui.button{part = 'follow', bind = {text = 'followText', variant = 'followVariant'}, text = 'Follow', variant = 'primary', height = 48, style = {metrics = {controlPaddingX = space.m}}},
    }, estimatedSize = 80}
end

-- Fills the texts a row of a person binds.
function Widgets.describePerson(person)
    person.type = 'person'
    person.followText = person.followed and 'Following' or 'Follow'
    person.followVariant = person.followed and 'default' or 'primary'
    return person
end

-- The template of a story in the row at the top of the feed: the face inside the ring and the handle.
function Widgets.story()
    local size = Style.size.story
    return {template = ui.column{gap = Style.space.xs, width = size + Style.space.m, padding = {Style.space.xs, 0},
        ui.stack{width = size, height = size, align = 'center',
            ui.image{part = 'ring', bind = {image = 'ring'}, image = 'images/ring.svg', align = 'stretch', accessibilityHidden = true},
            ui.avatar{part = 'avatar', bind = {image = 'avatar'}, size = size - 18, align = 'center'},
            ui.image{part = 'add', bind = {visible = 'mine'}, image = 'images/story-add.svg', width = 40, height = 40, align = 'end', visible = false, accessibilityHidden = true},
        },
        ui.label{part = 'name', bind = {text = 'name'}, font = 'caption', textAlign = 'center', align = 'stretch', wrap = false},
    }}
end

-- The template of a picture in a grid, with a mark on posts of several pictures.
function Widgets.tile()
    return {template = ui.stack{align = 'stretch',
        ui.image{part = 'photo', bind = {image = 'photo'}, image = Widgets.placeholder, fit = 'cover', align = 'stretch'},
        ui.row{align = 'stretch', alignItems = 'start', padding = Style.space.xs, ui.spacer{grow = 1}, ui.icon{part = 'pages', bind = {visible = 'hasPages'}, image = Shell.icon('image'), size = 28, color = 'onAccent', visible = false}},
    }}
end

-- The items of a grid of pictures for a list of posts.
function Widgets.tiles(posts)
    local items = {}
    for index, post in ipairs(posts) do
        items[index] = {id = post.id, type = 'tile', photo = post.photo, hasPages = post.pages ~= nil}
    end
    return items
end

return Widgets
