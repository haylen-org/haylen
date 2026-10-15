-- The frame of the app: the tabs, the navigation between screens, the notices and the layout that follows the size of the window. Phones show a tab bar at the bottom, and wider windows show a rail on the side.
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Shell = {}

Shell.tabs = {
    {id = 'home', text = 'Ride', icon = 'car', screen = 'screens.home'},
    {id = 'activity', text = 'Activity', icon = 'receipt', screen = 'screens.activity'},
    {id = 'wallet', text = 'Wallet', icon = 'wallet', screen = 'screens.wallet'},
    {id = 'account', text = 'Account', icon = 'user', screen = 'screens.account'},
}
-- The width in UI units from which the window shows the rail instead of the tab bar.
Shell.wideWidth = 960
Shell.pushTransition = {duration = 0.3, effect = 'slideIn', direction = 'left', ease = 'cubicOut'}
Shell.backTransition = {duration = 0.26, effect = 'slideIn', direction = 'right', ease = 'cubicOut'}
Shell.flatBar = {metrics = {panelRadius = 0}}
Shell.current = 'home'

function Shell.icon(name)
    return 'icons/' .. name .. '.svg'
end

-- Returns the width of the safe area in UI units.
function Shell.width()
    local _, unit = ui.scale()
    return viewport.safeRect().width / unit
end

function Shell.isWide()
    return Shell.width() >= Shell.wideWidth
end

-- Mounts the notices, which stay above every screen, and shows the first tab.
function Shell.start()
    Shell.notices = ui.mount(ui.toast{id = 'notice', position = 'bottom', duration = 2.5}, {layer = 10, sharedFocus = true})
    Shell.showTab('home')
end

function Shell.notify(text, tone)
    Shell.notices:command('notice', 'show', {text = text, tone = tone or 'success'})
end

-- Shows a tab as the only screen, without a transition, the way native tab bars switch. The parameters reach the screen of the tab.
function Shell.showTab(id, params)
    for _, tab in ipairs(Shell.tabs) do
        if tab.id == id then
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

-- Pushes a screen over the current one with a slide.
function Shell.open(Screen, params)
    window.setBackLeavesApp(false)
    return scene.push(Screen(params), Shell.pushTransition)
end

-- Puts a screen in the place of the current one with the same slide, such as the route after the search.
function Shell.replace(Screen, params)
    return scene.replace(Screen(params), Shell.pushTransition)
end

-- Goes back one screen, or to the home tab from the root of another tab.
function Shell.back()
    if scene.size() > 1 then
        scene.pop(Shell.backTransition)
        if scene.size() == 2 then
            window.setBackLeavesApp(Shell.current == 'home')
        end
    elseif Shell.current ~= 'home' then
        Shell.showTab('home')
    end
end

-- Shows the rail or the tab bar of a screen built with `Shell.frame`, and tells the screen when the window crosses the wide width.
function Shell.adapt(page)
    local wide = Shell.isWide()
    if page.wide == wide then
        return
    end
    page.wide = wide
    if page.gui:has('rail') then
        page.gui:set('rail', {visible = wide})
        page.gui:set('tabBar', {visible = not wide})
    end
    if page.layoutChanged then
        page:layoutChanged(wide)
    end
end

function Shell.tabButton(tab, current, placement)
    local selected = tab.id == current
    return ui.button{
        id = placement .. '-' .. tab.id,
        text = tab.text,
        icon = Shell.icon(tab.icon),
        iconPlacement = placement == 'bar' and 'top' or 'start',
        justify = placement == 'bar' and 'center' or 'start',
        variant = 'toolbar',
        grow = placement == 'bar' and 1 or 0,
        align = placement == 'bar' and 'center' or 'stretch',
        checked = selected and placement == 'rail',
        iconColor = selected and 'accentText' or 'textMuted',
        style = {colors = {text = ui.themeColor(selected and 'accentText' or 'textMuted'):toHex()}},
        accessibilityLabel = tab.text,
        onClick = function()
            if not selected then
                Shell.showTab(tab.id)
            end
        end,
    }
end

-- Wraps the content of a tab with the rail and the tab bar.
function Shell.frame(current, content, onCancel)
    local rail = {width = 260, gap = 6, align = 'stretch', padding = {28, 16}, style = Shell.flatBar,
        ui.row{gap = 12, padding = {0, 12, 20, 12},
            ui.image{image = 'images/logo.svg', width = 44, height = 44},
            ui.label{text = 'Ride Hailing', font = 'heading', wrap = false},
        },
    }
    local bar = {gap = 0}
    for _, tab in ipairs(Shell.tabs) do
        rail[#rail + 1] = Shell.tabButton(tab, current, 'rail')
        bar[#bar + 1] = Shell.tabButton(tab, current, 'bar')
    end
    return ui.row{gap = 0, onCancel = onCancel or Shell.back,
        ui.row{id = 'rail', visible = false, gap = 0, align = 'stretch', ui.panel(rail), ui.divider{vertical = true, align = 'stretch'}},
        ui.column{grow = 1, gap = 0, align = 'stretch',
            content,
            ui.column{id = 'tabBar', gap = 0, ui.divider{}, ui.panel{padding = {6, 8, 4, 8}, style = Shell.flatBar, ui.row(bar)}},
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

-- A bar at the top of a screen: a back button, a title and optional trailing nodes.
function Shell.header(title, trailing, back)
    local row = {gap = 8, padding = {10, 12}, height = 76}
    if back ~= false then
        row[#row + 1] = ui.button{id = 'back', icon = Shell.icon('back'), variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = Shell.back}
    end
    row[#row + 1] = ui.label{id = 'title', text = title, font = 'heading', wrap = false, grow = 1}
    for _, node in ipairs(trailing or {}) do
        row[#row + 1] = node
    end
    return ui.row(row)
end

return Shell
