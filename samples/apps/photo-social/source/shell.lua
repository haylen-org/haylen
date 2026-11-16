-- The frame of the app: the tabs, the routes of its links, the messages it shows and the layout that follows the width of the window. Phones show a bar of icons at the bottom, and wide windows show a rail with the wordmark and the names of the tabs on the left. The frame mounts once in the scene of the app, and the screens of each tab live in a stack of their own in its middle, so going to another screen changes only the middle while the bars stay. A new post and the stories open over everything as presentations, a new flow with bars of their own.
local async = require('async')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local system = require('haylen.system')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Style = require('style')

local Shell = {}

Shell.tabs = {
    {id = 'home', text = 'Home', icon = 'home', screen = 'screens.home'},
    {id = 'explore', text = 'Explore', icon = 'explore', screen = 'screens.explore'},
    {id = 'create', text = 'New post', image = 'images/create.svg'},
    {id = 'messages', text = 'Messages', icon = 'messages', screen = 'screens.messages'},
    {id = 'profile', text = 'Profile', screen = 'screens.profile'},
}

-- The scene of the app, which owns the frame and its navigation, paints the background of the theme behind them and keeps the frame current every frame.
Shell.scene = {}

function Shell.icon(name)
    return 'icons/' .. name .. '.svg'
end

-- Returns the width of the safe area in UI units.
function Shell.width()
    local _, unit = ui.scale()
    return viewport.safeRect().width / unit
end

function Shell.start()
    scene.push(Shell.scene)
end

-- Mounts the frame and the tabs in the middle of the frame, starts the routes of the links over the tabs, and shows a banner for each answer that arrives in a chat that does not show.
function Shell.scene:enter()
    Shell.gui = ui.mount(Shell.frame(), {owner = self, sharedFocus = true})
    Shell.adaptFrame()
    local tabs = {}
    for _, tab in ipairs(Shell.tabs) do
        if tab.screen then
            tabs[#tabs + 1] = {id = tab.id, root = function() return require(tab.screen)() end}
        end
    end
    Shell.navigation = ui.navigationTabs{gui = Shell.gui, id = 'middle', tabs = tabs, owner = self, onChange = function(event)
        Shell.select(event.selected, event.previous)
    end}
    Shell.router = require('routes').start(Shell.navigation, self)
    events.on('chatChanged', Shell.showUnread, {owner = self})
    events.on('chatAnswered', Shell.showAnswer, {owner = self})
    Shell.showUnread()
end

function Shell.scene:update()
    Shell.adaptFrame()
end

function Shell.scene:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), ui.themeColor('window'))
end

-- Confirms what the person just did in a toast at the bottom.
function Shell.notify(text, tone)
    ui.showMessage{text = text, tone = tone or 'neutral', indicator = 'icon', owner = Shell.scene}
end

-- Tells that something went away in a snackbar whose action brings it back.
function Shell.offerUndo(text, undo)
    ui.showMessage{style = 'snackbar', text = text, action = 'Undo', owner = Shell.scene, onAction = undo}
end

-- Asks before something goes away for good in an alert, and calls `confirmed` once the person picks the destructive action.
function Shell.confirm(title, message, action, confirmed)
    return ui.presentAlert{title = title, message = message, owner = Shell.scene,
        actions = {{id = 'keep', text = 'Cancel', role = 'cancel'}, {id = 'confirm', text = action, role = 'destructive'}},
        onDismiss = function(event)
            if event.result == 'confirm' then
                confirmed()
            end
        end}
end

-- Puts a link on the clipboard and tells whether it got there, since a browser keeps the clipboard from a page without a recent input of the person.
function Shell.copy(link)
    async.spawn(function()
        local _, problem = system.setClipboard(link):await()
        if problem then
            Shell.notify(problem, 'warning')
            return
        end
        Shell.notify('The link is on the clipboard.')
    end)
end

-- Drops a banner with the face of whoever answered under the status bar, and a tap on it opens their chat through its route, in the messages tab beside or over the conversations.
function Shell.showAnswer(conversation)
    local answer = conversation.messages[#conversation.messages]
    ui.showMessage{style = 'banner', title = conversation.name, text = answer.text, icon = conversation.avatar, owner = Shell.scene, onTap = function()
        Shell.router:open(require('routes').chat(conversation.person))
    end}
end

-- Shows a tab with the screens it kept, or goes back to its root when it already shows. The tab of a new post presents the new post instead.
function Shell.showTab(id)
    if id == 'create' then
        Shell.compose()
        return
    end
    Shell.navigation:select(id)
end

-- The screen that the person sees: the screen on top of the presentation that shows, or the top screen of the tab that shows.
function Shell.top()
    local presented = Shell.presented
    if presented and not presented.dismissed then
        local screen = presented.screen
        return screen.steps and screen.steps.top or screen
    end
    local top = Shell.navigation.current.top
    local split = top.split
    if not split then
        return top
    end
    if split.hasDetail and not split.collapsed then
        return split.detail.top
    end
    return split.primary.top
end

-- Shows the screen of a location, such as "profile/ava", in the stack of the tab that shows. A screen that already shows the same location further down the stack, such as the profile that a post opened from, comes back instead, so the stack never holds two copies of it.
function Shell.open(location)
    local stack = Shell.navigation.current
    local screens = stack.screens
    for level = #screens - 1, 1, -1 do
        local shown = screens[level]
        if Shell.router:location(shown) == location then
            stack:popTo(shown)
            return
        end
    end
    Shell.router:push(location)
end

-- Presents a new post over everything, a flow of its own with its bar and its steps.
function Shell.compose()
    Shell.present(require('screens.new-post')())
end

-- Presents the stories over everything on their dark stage, from the story at the index, and calls back once they close.
function Shell.openStory(index, closed)
    Shell.present(require('screens.story')({index = index}), closed, Style.color.stage)
end

-- Presents a flow over the whole screen, on the color of the window of the app or another color, while no other presentation shows.
function Shell.present(screen, closed, color)
    if Shell.presented and not Shell.presented.dismissed then
        return
    end
    Shell.presented = ui.present(screen, {style = 'fullScreen', color = color or ui.themeColor('window'), owner = Shell.scene, onDismiss = closed})
end

-- Shows the rail on a wide window and the bar of icons otherwise.
function Shell.adaptFrame()
    local wide = Shell.width() >= Style.wideWidth
    if Shell.wide == wide then
        return
    end
    Shell.wide = wide
    Shell.gui:set('rail', {visible = wide})
    Shell.gui:set('tabBar', {visible = not wide})
end

-- Tells a screen when the window crosses the widths of the wide and roomy layouts.
function Shell.adapt(page)
    local width = Shell.width()
    local wide, roomy = width >= Style.wideWidth, width >= Style.roomyWidth
    if page.wide == wide and page.roomy == roomy then
        return
    end
    page.wide, page.roomy = wide, roomy
    if page.layoutChanged then
        page:layoutChanged(wide, roomy)
    end
end

-- The picture of a tab: its icon, filled while it shows, the round button of a new post in the colors of the app, or the face of the person for the profile.
function Shell.tabIcon(tab, selected)
    if tab.id == 'profile' then
        local me = require('data.social').me
        return selected and me.avatarRing or me.avatar
    end
    if tab.image then
        return tab.image
    end
    return Shell.icon(tab.icon .. (selected and '-filled' or ''))
end

function Shell.tabStyle(selected)
    return {fonts = {button = {size = Style.type.body, bold = selected}}, metrics = {iconSize = 32, controlPaddingX = Style.space.m, contentSpacing = Style.space.m}}
end

function Shell.tabButton(tab, placement)
    local selected = tab.id == Shell.tabs[1].id
    local rail = placement == 'rail'
    -- The face of the profile and the button of a new post keep their colors, and the icons take the color of the text. The button of a new post stands out in the bar.
    local iconColor = 'text'
    local style = Shell.tabStyle(selected)
    if tab.id == 'profile' then
        iconColor = false
    elseif tab.image then
        iconColor = false
        style.metrics.iconSize = rail and 32 or 48
    end
    local button = ui.button{
        id = placement .. '-' .. tab.id,
        text = rail and tab.text or nil,
        icon = Shell.tabIcon(tab, selected),
        iconColor = iconColor,
        variant = 'toolbar',
        justify = rail and 'start' or 'center',
        grow = rail and 0 or 1,
        align = rail and 'stretch' or 'center',
        height = rail and 64 or Style.size.tabBar - Style.space.s,
        accessibilityLabel = tab.text,
        tooltip = not rail and tab.text or nil,
        style = style,
        onClick = function()
            Shell.showTab(tab.id)
        end,
    }
    if tab.id ~= 'messages' then
        return button
    end
    -- The messages tab carries the count of unread messages, at the end of its row in the rail and over its icon in the bar.
    if rail then
        return ui.stack{align = 'stretch',
            button,
            ui.row{gap = 0, align = 'stretch', justify = 'end', padding = {0, Style.space.m, 0, 0}, ui.badge{id = 'rail-unread', text = '', tone = 'danger', solid = true, visible = false}},
        }
    end
    return ui.stack{align = 'center', grow = 1,
        button,
        ui.row{gap = 0, align = 'center', padding = {0, 0, 22, 44}, ui.badge{id = 'bar-unread', text = '', tone = 'danger', solid = true, visible = false}},
    }
end

-- Shows the count of unread messages on the tab of the messages.
function Shell.showUnread()
    local count = require('chats').unreadCount()
    Shell.gui:set('rail-unread', {text = count, visible = count > 0})
    Shell.gui:set('bar-unread', {text = count, visible = count > 0})
end

-- Marks the tab that shows in the rail and in the bar, changing only the look of the two tabs.
function Shell.select(current, previous)
    for _, tab in ipairs(Shell.tabs) do
        if tab.id == current or tab.id == previous then
            local selected = tab.id == current
            local look = {icon = Shell.tabIcon(tab, selected), style = Shell.tabStyle(selected)}
            Shell.gui:set('rail-' .. tab.id, look)
            Shell.gui:set('bar-' .. tab.id, look)
        end
    end
end

-- The wordmark: the logo and the name of the app.
function Shell.wordmark(size)
    return ui.row{gap = Style.space.s,
        ui.image{image = 'images/logo.svg', width = size, height = size, accessibilityHidden = true},
        ui.label{text = 'Photo Social', font = 'title', wrap = false, style = Style.text('title', Style.type.heading)},
    }
end

-- The frame: the rail with the wordmark and the tabs, the middle where the stacks of the tabs show their screens, and the bar of icons.
function Shell.frame()
    local space = Style.space
    local surface = {colors = {panel = 'window'}, metrics = {panelRadius = 0}}
    local rail = {id = 'railTabs', gap = space.xs, align = 'stretch'}
    local bar = {id = 'barTabs', gap = 0, height = Style.size.tabBar, padding = {0, space.s}}
    for _, tab in ipairs(Shell.tabs) do
        rail[#rail + 1] = Shell.tabButton(tab, 'rail')
        bar[#bar + 1] = Shell.tabButton(tab, 'bar')
    end
    return ui.row{gap = 0,
        ui.row{id = 'rail', visible = false, gap = 0, align = 'stretch',
            ui.panel{width = Style.size.rail, gap = space.xs, padding = {space.xl, space.m, space.l, space.m}, align = 'stretch', style = surface,
                ui.row{padding = {0, space.s, space.xl, space.s}, Shell.wordmark(52)},
                ui.column(rail),
            },
            ui.divider{vertical = true, align = 'stretch'},
        },
        ui.column{grow = 1, gap = 0, align = 'stretch',
            ui.column{id = 'middle', grow = 1, gap = 0, align = 'stretch'},
            ui.panel{id = 'tabBar', gap = 0, padding = 0, style = surface, ui.divider{}, ui.row(bar)},
        },
    }
end

-- Centers a node in the width it gets, as wide as it may be up to `maxWidth`. A `grow` lets the row take the free height of a column.
function Shell.centered(maxWidth, node, grow)
    node.grow = 100
    node.maxWidth = maxWidth
    node.align = 'stretch'
    return ui.row{gap = 0, grow = grow or 0, align = 'stretch', ui.spacer{grow = 1}, node, ui.spacer{grow = 1}}
end

-- A bar at the top of a pushed screen: a back button, which goes back the way the system back does, a title and optional trailing nodes.
function Shell.header(title, trailing)
    local row = {gap = Style.space.xs, padding = {0, Style.space.s}, height = Style.size.header}
    row[#row + 1] = ui.button{id = 'back', icon = Shell.icon('back'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = ui.goBack}
    row[#row + 1] = ui.label{id = 'title', text = title, font = 'heading', wrap = false, grow = 1}
    for _, node in ipairs(trailing or {}) do
        row[#row + 1] = node
    end
    return ui.column{gap = 0, ui.row(row), ui.divider{}}
end

return Shell
