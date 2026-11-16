-- The frame of the app: the tabs, the navigation between screens, the notices and the layout that follows the width of the window. Phones show a bar of icons at the bottom, and wide windows show a rail with the wordmark and the names of the tabs on the left. The frame mounts once, and every screen lays out in its middle, so opening a screen changes only the middle and never builds the bars again.
local events = require('haylen.events')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Style = require('style')

local Shell = {}

Shell.tabs = {
    {id = 'home', text = 'Home', icon = 'home', screen = 'screens.home'},
    {id = 'explore', text = 'Explore', icon = 'explore', screen = 'screens.explore'},
    {id = 'create', text = 'New post', image = 'images/create.svg', screen = 'screens.compose-pick', push = true},
    {id = 'messages', text = 'Messages', icon = 'messages', screen = 'screens.inbox'},
    {id = 'profile', text = 'Profile', screen = 'screens.profile'},
}
Shell.pushTransition = {duration = 0.28, effect = 'slideIn', direction = 'left', ease = 'cubicOut'}
Shell.backTransition = {duration = 0.24, effect = 'slideIn', direction = 'right', ease = 'cubicOut'}
Shell.riseTransition = {duration = 0.3, effect = 'moveIn', direction = 'up', ease = 'cubicOut'}
Shell.current = 'home'

function Shell.icon(name)
    return 'icons/' .. name .. '.svg'
end

-- Returns the width of the safe area in UI units.
function Shell.width()
    local _, unit = ui.scale()
    return viewport.safeRect().width / unit
end

-- Mounts the frame and the notices, which stay above every screen, and shows the first tab.
function Shell.start()
    Shell.gui = ui.mount(Shell.frame(), {layer = 1, sharedFocus = true})
    Shell.notices = ui.mount(ui.toast{id = 'notice', position = 'bottom', duration = 2.5, indicator = 'icon'}, {layer = 10, sharedFocus = true})
    Shell.adaptFrame()
    Shell.showTab('home')
    Shell.followUnread()
end

-- The placement of the GUI of a screen: the middle of the frame, between the rail or the bar of icons.
function Shell.middle()
    return {gui = Shell.gui, id = 'middle'}
end

function Shell.notify(text, tone)
    Shell.notices:command('notice', 'show', {text = text, tone = tone or 'neutral'})
end

-- Shows a tab as the only screen, without a transition, the way native tab bars switch, or pushes the screen of a tab that opens over the others, such as a new post.
function Shell.showTab(id, params)
    for _, tab in ipairs(Shell.tabs) do
        if tab.id == id then
            if tab.push then
                Shell.open(require(tab.screen), params, Shell.riseTransition)
                return
            end
            Shell.select(id)
            if scene.size() > 1 then
                scene.popToRoot()
            end
            scene.replace(require(tab.screen)(params))
            window.setBackLeavesApp(id == 'home')
            return
        end
    end
end

-- Pushes a screen over the current one with a slide, or with another transition. A screen of the same kind that already shows the same thing, named by the parameter `key`, such as the chat with a person that a profile opened from that chat, comes back instead, so the stack never holds two copies of it.
function Shell.open(Screen, params, transition, key)
    if key then
        local screens = scene.list()
        for level = #screens - 1, 1, -1 do
            local shown = screens[level]
            if shown and shown:is(Screen) and shown.params[key] == params[key] then
                scene.popTo(level, Shell.backTransition)
                window.setBackLeavesApp(level == 1 and Shell.current == 'home')
                return
            end
        end
    end
    window.setBackLeavesApp(false)
    return scene.push(Screen(params), transition or Shell.pushTransition)
end

-- Puts a screen in the place of the current one with the same slide, such as the next step of a new post.
function Shell.replace(Screen, params)
    return scene.replace(Screen(params), Shell.pushTransition)
end

-- Goes back one screen, or to the home tab from the root of another tab.
function Shell.back()
    if scene.size() > 1 then
        scene.pop(scene.top().backTransition or Shell.backTransition)
        if scene.size() == 2 then
            window.setBackLeavesApp(Shell.current == 'home')
        end
    elseif Shell.current ~= 'home' then
        Shell.showTab('home')
    end
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

-- Keeps the frame current and tells a screen when the window crosses the widths of the wide and roomy layouts.
function Shell.adapt(page)
    Shell.adaptFrame()
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

function Shell.tabButton(tab, current, placement)
    local selected = tab.id == current
    local rail = placement == 'rail'
    -- The face of the profile and the button of a new post keep their colors, and the icons take the color of the text. The button of a new post stands out in the bar.
    local iconColor = 'text'
    local iconSize = 32
    if tab.id == 'profile' then
        iconColor = false
    elseif tab.image then
        iconColor = false
        iconSize = rail and 32 or 48
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
        style = {fonts = {button = {size = Style.type.body, bold = selected}}, metrics = {iconSize = iconSize, controlPaddingX = Style.space.m, contentSpacing = Style.space.m}},
        onClick = function()
            if not selected or tab.push or scene.size() > 1 then
                Shell.showTab(tab.id)
            end
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

-- Keeps the count of unread messages current while the frame lives.
function Shell.followUnread()
    events.on('chatChanged', Shell.showUnread, {owner = Shell.gui})
    Shell.showUnread()
end

-- Marks the tab that shows, in the rail and in the bar, and keeps the count of unread messages on the tabs built again.
function Shell.select(id)
    if Shell.current == id and Shell.gui:has('rail-' .. id) then
        return
    end
    Shell.current = id
    local rail, bar = {}, {}
    for _, tab in ipairs(Shell.tabs) do
        rail[#rail + 1] = Shell.tabButton(tab, id, 'rail')
        bar[#bar + 1] = Shell.tabButton(tab, id, 'bar')
    end
    Shell.gui:replaceChildren('railTabs', rail)
    Shell.gui:replaceChildren('barTabs', bar)
    Shell.showUnread()
end

-- The wordmark: the logo and the name of the app.
function Shell.wordmark(size)
    return ui.row{gap = Style.space.s,
        ui.image{image = 'images/logo.svg', width = size, height = size, accessibilityHidden = true},
        ui.label{text = 'Photo Social', font = 'title', wrap = false, style = Style.text('title', Style.type.heading)},
    }
end

-- The frame: the rail with the wordmark and the tabs, the middle where the screens lay out, and the bar of icons. The bars paint the window color, so the screens that slide in the middle pass under them. The tabs arrive with `Shell.select`.
function Shell.frame()
    local space = Style.space
    local surface = {colors = {panel = 'window'}, metrics = {panelRadius = 0}}
    return ui.row{gap = 0, onCancel = Shell.back,
        ui.row{id = 'rail', visible = false, gap = 0, align = 'stretch',
            ui.panel{width = Style.size.rail, gap = space.xs, padding = {space.xl, space.m, space.l, space.m}, align = 'stretch', style = surface,
                ui.row{padding = {0, space.s, space.xl, space.s}, Shell.wordmark(52)},
                ui.column{id = 'railTabs', gap = space.xs, align = 'stretch'},
            },
            ui.divider{vertical = true, align = 'stretch'},
        },
        ui.column{grow = 1, gap = 0, align = 'stretch',
            ui.column{id = 'middle', grow = 1, gap = 0},
            ui.panel{id = 'tabBar', gap = 0, padding = 0, style = surface, ui.divider{}, ui.row{id = 'barTabs', gap = 0, height = Style.size.tabBar, padding = {0, space.s}}},
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

-- A bar at the top of a pushed screen: a back button, a title and optional trailing nodes.
function Shell.header(title, trailing, options)
    options = options or {}
    local row = {gap = Style.space.xs, padding = {0, Style.space.s}, height = Style.size.header}
    row[#row + 1] = ui.button{id = 'back', icon = Shell.icon(options.close and 'close' or 'back'), iconColor = 'text', variant = 'icon', accessibilityLabel = options.close and 'Close' or 'Back', tooltip = options.close and 'Close' or 'Back', onClick = options.onBack or Shell.back}
    row[#row + 1] = ui.label{id = 'title', text = title, font = 'heading', wrap = false, grow = 1}
    for _, node in ipairs(trailing or {}) do
        row[#row + 1] = node
    end
    return ui.column{gap = 0, ui.row(row), ui.divider{}}
end

return Shell
