-- The frame of the app: the stack of its screens, the notices and the parts that float over the map. The map fills the screen and has no tab bar: a round button opens the menu with the trips, the wallet and the account over the map, and every step of a ride shows in a sheet, at the bottom of a phone or as a panel beside the map on a wide window. The steps of a ride and the pages of the menu are screens of one stack over the whole screen, and the menu is a drawer presented over everything.
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Style = require('style')

local Shell = {}

-- The steps of a ride change their sheet over the map with a cross fade, and the drawer of the menu slides in from the start of the screen.
Shell.sheetTransition = {effect = 'fade', duration = 0.22}
Shell.menuTransition = {effect = 'slide', duration = 0.24}

-- The scene of the app, which owns the frame and its stack and paints the background of the theme behind them.
Shell.scene = {}

function Shell.icon(name)
    return 'icons/' .. name .. '.svg'
end

-- Returns the width of the safe area in UI units.
function Shell.width()
    local _, unit = ui.scale()
    return viewport.safeRect().width / unit
end

function Shell.isWide()
    return Shell.width() >= Style.wideWidth
end

function Shell.start()
    scene.push(Shell.scene)
end

-- Mounts the frame, whose middle is the whole screen, with the stack of the screens on the map, and the notices, which stay above every screen.
function Shell.scene:enter()
    Shell.gui = ui.mount(ui.column{id = 'middle', grow = 1, gap = 0, align = 'stretch'}, {owner = self, placement = 'screen', sharedFocus = true})
    Shell.notices = ui.mount(ui.toast{id = 'notice', position = 'top', duration = 2.5, indicator = 'icon'}, {owner = self, layer = 10, sharedFocus = true})
    Shell.stack = ui.navigationStack{gui = Shell.gui, id = 'middle', root = require('screens.home')(), owner = self}
end

function Shell.scene:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), ui.themeColor('window'))
end

function Shell.notify(text, tone)
    Shell.notices:command('notice', 'show', {text = text, tone = tone or 'neutral'})
end

-- Shows the map with the sheet that asks where to.
function Shell.showHome()
    Shell.closeMenu()
    Shell.stack:popToRoot()
end

-- The screen that the person sees: the menu while it shows, or the top screen of the stack.
function Shell.top()
    if Shell.menu and not Shell.menu.dismissed then
        return Shell.menu.screen
    end
    return Shell.stack.top
end

-- Pushes a screen over the current one with a slide, or with another transition.
function Shell.open(Screen, params, transition)
    Shell.stack:push(Screen(params), transition)
end

-- Puts a screen in the place of the current one, such as the trip after its request, with the cross fade of the sheets over the map.
function Shell.replace(Screen, params)
    Shell.stack:replace(Screen(params), Shell.sheetTransition)
end

-- Opens a page of the menu, or a new ride from a past trip, right over the map, so going back always returns to the map.
function Shell.openPage(Screen, params)
    Shell.closeMenu()
    Shell.stack:setScreens({Shell.stack.screens[1], Screen(params)})
end

-- Presents the menu as a drawer over the dimmed map, which a tap beside it, Escape and the back button close.
function Shell.openMenu()
    if Shell.menu and not Shell.menu.dismissed then
        return
    end
    Shell.menu = ui.present(require('screens.menu')(), {style = 'dialog', transition = Shell.menuTransition, owner = Shell.scene})
end

function Shell.closeMenu()
    if Shell.menu and not Shell.menu.dismissed then
        Shell.menu:dismiss()
    end
end

-- Tells a screen when the window crosses the wide width.
function Shell.adapt(page)
    local wide = Shell.isWide()
    if page.wide == wide then
        return
    end
    page.wide = wide
    if page.layoutChanged then
        page:layoutChanged(wide)
    end
end

-- A round white button that floats over the map, with a soft shadow of its own.
function Shell.mapButton(id, icon, label, onClick, properties)
    local size = Style.size.mapButton
    local button = {id = id, icon = Shell.icon(icon), iconColor = 'text', width = size, height = size, accessibilityLabel = label, tooltip = label, onClick = onClick,
        style = {colors = {raised = Style.color.paper, border = Style.color.paper}, metrics = {controlRadius = size / 2, controlPaddingX = 0}}}
    for key, value in pairs(properties or {}) do
        button[key] = value
    end
    local card = {padding = Style.space.xxs, align = button.align or 'start', anchor = button.anchor, margin = button.margin, visible = button.visible, style = {metrics = {panelRadius = size / 2 + Style.space.xxs}}}
    button.align, button.anchor, button.margin, button.visible = nil, nil, nil, nil
    card[1] = ui.button(button)
    return ui.card(card)
end

-- The sheet of a step of a ride: a white card with large round corners that floats at the bottom of a phone, with a handle, or as a panel at the top left of a wide window.
function Shell.sheet(id, children, wide)
    local space = Style.space
    local sheet = {id = id, gap = space.m, padding = wide and {space.l, space.l, space.l, space.l} or {space.s, space.l, space.l, space.l}}
    if not wide then
        sheet[1] = ui.panel{width = 56, height = 6, padding = 0, align = 'center', margin = {0, 0, space.xxs, 0}, style = {colors = {panel = Style.color.lineStrong}, metrics = {panelRadius = 3}}, accessibilityHidden = true}
    end
    for _, child in ipairs(children) do
        sheet[#sheet + 1] = child
    end
    if wide then
        sheet.anchor, sheet.width, sheet.margin = 'topLeft', Style.size.panel, {space.l, space.l}
    else
        sheet.anchor, sheet.margin = 'stretchBottom', {0, space.xs, space.xs, space.xs}
    end
    return ui.card(sheet)
end

-- Centers a node in the width it gets, as wide as it may be up to `maxWidth`. A `grow` lets the row take the free height of a column.
function Shell.centered(maxWidth, node, grow)
    node.grow = 100
    node.maxWidth = maxWidth
    node.align = 'stretch'
    return ui.row{gap = 0, grow = grow or 0, align = 'stretch', ui.spacer{grow = 1}, node, ui.spacer{grow = 1}}
end

-- A bar at the top of a page of the menu: a back button, a title and optional trailing nodes.
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
