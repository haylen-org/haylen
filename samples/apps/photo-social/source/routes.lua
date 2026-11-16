-- The routes of the app: the location of every screen that a link opens, such as "profile/ava", "post/12" or "chat/kai". The links of the scheme of the app, "haylenphotos://profile/ava", and of its web host, "https://photos.example.com/profile/ava", open them from a cold start and while the app runs, and the screens that the person opens by hand go through the same routes, so the router knows where every screen leads and builds the navigation again after the system ended the app. A chat is the detail of the split of the messages tab.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Chats = require('chats')
local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')

local Routes = {}

Routes.scheme = 'haylenphotos'
Routes.host = 'photos.example.com'

-- What a link to something the app does not hold shows, with the way back.
local Missing = haylen.class('Missing', Page)

function Missing:tree()
    return ui.column{grow = 1, gap = 0,
        Shell.header(''),
        ui.emptyState{grow = 1, image = Shell.icon('image'), imageSize = 72, title = 'Nothing here', message = self.params.text},
    }
end

-- Creates the router over the tabs of the app, which lives as long as its owner.
function Routes.start(tabs, owner)
    return ui.router{tabs = tabs, schemes = {Routes.scheme}, hosts = {Routes.host}, restore = true, owner = owner, routes = {
        {name = 'home', path = 'home', tab = 'home'},
        {name = 'explore', path = 'explore', tab = 'explore'},
        {name = 'messages', path = 'messages', tab = 'messages'},
        {name = 'me', path = 'profile', tab = 'profile'},
        {name = 'profile', path = 'profile/{id}', screen = Routes.profileScreen},
        {name = 'post', path = 'post/{id:integer}', screen = Routes.postScreen},
        {name = 'chat', path = 'chat/{person}', tab = 'messages', detail = true, screen = Routes.chatScreen},
    }}
end

function Routes.profile(id)
    return 'profile/' .. id
end

-- The location of a post, from the number of its id.
function Routes.post(post)
    return 'post/' .. post.id:match('(%d+)$')
end

function Routes.chat(personId)
    return 'chat/' .. personId
end

function Routes.link(location)
    return Routes.scheme .. '://' .. location
end

function Routes.webLink(location)
    return 'https://' .. Routes.host .. '/' .. location
end

function Routes.profileScreen(parameters)
    if not Social.person(parameters.id) then
        return Missing({text = 'Nobody on the app goes by this name.'})
    end
    return require('screens.profile')({person = parameters.id})
end

function Routes.postScreen(parameters)
    local id = 'post-' .. parameters.id
    if not Social.post(id) then
        return Missing({text = 'This post is gone.'})
    end
    return require('screens.post')({post = id})
end

-- The chat with a person, which starts the conversation when there is none yet.
function Routes.chatScreen(parameters)
    local person = Social.person(parameters.person)
    if not person or person.me then
        return Missing({text = 'Nobody on the app goes by this name.'})
    end
    return require('screens.chat')({conversation = Chats.with(person).id})
end

return Routes
