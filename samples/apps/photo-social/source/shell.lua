-- The frame of the app: the tabs, the navigation between screens, the notices and the layout that follows the width of the window. Phones show a bar of icons at the bottom, and wide windows show a rail with the wordmark and the names of the tabs on the left.
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

-- Mounts the notices, which stay above every screen, and shows the first tab.
function Shell.start()
    Shell.notices = ui.mount(ui.toast{id = 'notice', position = 'bottom', duration = 2.5, indicator = 'icon'}, {layer = 10, sharedFocus = true})
    Shell.showTab('home')
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
            Shell.current = id
            if scene.size() > 1 then
                scene.popToRoot()
            end
            scene.replace(require(tab.screen)(params))
            window.setBackLeavesApp(id == 'home')
            return
        end
    end
end

-- Pushes a screen over the current one with a slide, or with another transition.
function Shell.open(Screen, params, transition)
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

-- Tells a screen when the window crosses the widths of the wide and roomy layouts, and shows the rail or the bar of a screen built with `Shell.frame`.
function Shell.adapt(page)
    local width = Shell.width()
    local wide, roomy = width >= Style.wideWidth, width >= Style.roomyWidth
    if page.wide == wide and page.roomy == roomy then
        return
    end
    page.wide, page.roomy = wide, roomy
    if page.gui:has('rail') then
        page.gui:set('rail', {visible = wide})
        page.gui:set('tabBar', {visible = not wide})
    end
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
            if not selected or tab.push then
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

-- Shows the count of unread messages on the tab of the messages, and keeps it current while the page lives.
function Shell.followUnread(page)
    local function show()
        local count = require('chats').unreadCount()
        for _, placement in ipairs({'rail', 'bar'}) do
            if page.gui:has(placement .. '-unread') then
                page.gui:set(placement .. '-unread', {text = count, visible = count > 0})
            end
        end
    end
    events.on('chatChanged', show, {owner = page})
    show()
end

-- The wordmark: the logo and the name of the app.
function Shell.wordmark(size)
    return ui.row{gap = Style.space.s,
        ui.image{image = 'images/logo.svg', width = size, height = size, accessibilityHidden = true},
        ui.label{text = 'Photo Social', font = 'title', wrap = false, style = Style.text('title', Style.type.heading)},
    }
end

-- Wraps the content of a tab with the rail and the bar of icons.
function Shell.frame(current, content, onCancel)
    local space = Style.space
    local rail = {width = Style.size.rail, gap = space.xs, padding = {space.xl, space.m, space.l, space.m}, align = 'stretch',
        ui.row{padding = {0, space.s, space.xl, space.s}, Shell.wordmark(52)},
    }
    local bar = {gap = 0, height = Style.size.tabBar, padding = {0, space.s}}
    for _, tab in ipairs(Shell.tabs) do
        rail[#rail + 1] = Shell.tabButton(tab, current, 'rail')
        bar[#bar + 1] = Shell.tabButton(tab, current, 'bar')
    end
    return ui.row{gap = 0, onCancel = onCancel or Shell.back,
        ui.row{id = 'rail', visible = false, gap = 0, align = 'stretch', ui.column(rail), ui.divider{vertical = true, align = 'stretch'}},
        ui.column{grow = 1, gap = 0, align = 'stretch',
            content,
            ui.column{id = 'tabBar', gap = 0, ui.divider{}, ui.row(bar)},
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
