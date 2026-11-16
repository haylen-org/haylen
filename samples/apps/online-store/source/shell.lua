-- The frame of the app: the tabs, the routes of its links, the messages it shows and the layout that follows the width of the window. Phones show a bar of tabs with their names at the bottom, and wide windows show the wordmark, the tabs, the search and the bag across the top, the way a store on the web does. The frame mounts once in the scene of the app, and the screens of each tab live in a stack of their own in its middle, so going to another screen changes only the middle while the bars stay. The bag opens over everything as a sheet, a flow of its own up to the confirmation of the order.
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
    {id = 'home', text = 'Shop', icon = 'shop', screen = 'screens.shop'},
    {id = 'browse', text = 'Browse', icon = 'browse', screen = 'screens.browse'},
    {id = 'saved', text = 'Saved', icon = 'heart', screen = 'screens.saved'},
    {id = 'orders', text = 'Orders', icon = 'orders', screen = 'screens.orders-tab'},
    {id = 'account', text = 'Account', icon = 'user', screen = 'screens.account'},
}

-- The scene of the app, which owns the frame and its navigation, paints the paper of the store behind them and keeps the frame current every frame.
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

-- Mounts the frame and the tabs in the middle of the frame, and starts the routes of the links over the tabs. The bag button of the frame follows the bag while the app runs.
function Shell.scene:enter()
    Shell.gui = ui.mount(Shell.frame(), {owner = self, sharedFocus = true})
    -- The toasts and snackbars keep above the bars of the frame, so they never cover the tabs or the player.
    ui.setMessageArea{gui = Shell.gui, id = 'middle'}
    Shell.showBag(Shell)
    Shell.adaptFrame()
    local tabs = {}
    for index, tab in ipairs(Shell.tabs) do
        tabs[index] = {id = tab.id, root = function() return require(tab.screen)() end}
    end
    Shell.navigation = ui.navigationTabs{gui = Shell.gui, id = 'middle', tabs = tabs, owner = self, onChange = function(event)
        Shell.select(event.selected, event.previous)
    end}
    Shell.router = require('routes').start(Shell.navigation, self)
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

-- Offers the links of a location in an action sheet, which points at the button that opened it on a wide window, and puts the link the person picks on the clipboard.
function Shell.share(title, location, anchor)
    local Routes = require('routes')
    return ui.presentAlert{style = 'actionSheet', title = title, anchor = anchor, owner = Shell.scene,
        actions = {{id = 'web', text = 'Copy link'}, {id = 'app', text = 'Copy the link of the app'}, {id = 'cancel', text = 'Cancel', role = 'cancel'}},
        onDismiss = function(event)
            if event.result == 'web' then
                Shell.copy(Routes.webLink(location))
            elseif event.result == 'app' then
                Shell.copy(Routes.link(location))
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

-- Shows a tab with the screens it kept, or goes back to its root when it already shows. Parameters go to the root of the tab, such as the category that the browse tab shows, which then shows at its root.
function Shell.showTab(id, params)
    Shell.navigation:select(id)
    if not params then
        return
    end
    local stack = Shell.navigation.current
    stack:popToRoot()
    stack.screens[1]:apply(params)
end

-- Whether a presented screen, such as the flow of the bag, still shows.
function Shell.isPresented(screen)
    return screen ~= nil and screen.presentation ~= nil and not screen.presentation.dismissed
end

-- The screen that the person sees: the step on top of the flow of the bag while it shows, or the top screen of the tab that shows, which may be the open order beside the list.
function Shell.top()
    if Shell.isPresented(Shell.bagFlow) then
        return Shell.bagFlow.steps.top
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

-- Shows the screen of a location, such as "product/12", in the stack of the tab that shows. A screen that already shows the same location further down the stack, such as the product that a related product opened from, comes back instead, so the stack never holds two copies of it.
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

-- Presents the bag through its route over everything as a sheet that starts a flow of its own, from the bag to the checkout and the confirmation, while it does not show yet.
function Shell.openBag()
    if Shell.isPresented(Shell.bagFlow) then
        return
    end
    Shell.router:push('bag')
end

-- Closes the flow of the bag, such as once an order is placed.
function Shell.closeBag()
    if Shell.isPresented(Shell.bagFlow) then
        Shell.bagFlow.presentation:dismiss()
    end
end

-- Shows the navigation of the top on a wide window and the bar of tabs otherwise.
function Shell.adaptFrame()
    local wide = Shell.isWide()
    if Shell.wide == wide then
        return
    end
    Shell.wide = wide
    Shell.gui:set('topNav', {visible = wide})
    Shell.gui:set('tabBar', {visible = not wide})
end

-- Shows the bar of a phone at the top of a tab built with `Shell.tabPage`, and tells a screen when the window crosses the wide width.
function Shell.adapt(page)
    local wide = Shell.isWide()
    if page.wide == wide then
        return
    end
    page.wide = wide
    if page.gui:has('phoneBar') then
        page.gui:set('phoneBar', {visible = not wide})
    end
    -- The bag of the top of a wide window is the bag of every screen, so the header of a screen keeps its own only on a phone.
    if page.gui:has('headerBagBox') then
        page.gui:set('headerBagBox', {visible = not wide})
    end
    if page.layoutChanged then
        page:layoutChanged(wide)
    end
end

-- Marks the tab that shows in the navigation of the top and in the bar of tabs, changing only the look of the two tabs.
function Shell.select(current, previous)
    for _, tab in ipairs(Shell.tabs) do
        if tab.id == current or tab.id == previous then
            local selected = tab.id == current
            Shell.gui:set('bar-' .. tab.id, {icon = Shell.barIcon(tab, selected), iconColor = selected and 'text' or 'textMuted', style = Shell.barStyle(selected)})
            Shell.gui:set('nav-' .. tab.id, {style = Shell.navStyle(selected)})
            Shell.gui:set('navLine-' .. tab.id, {color = selected and 'text' or 'window'})
        end
    end
end

-- The wordmark: the logo and the name of the store in its serif.
function Shell.wordmark(size)
    return ui.row{gap = Style.space.s,
        ui.image{image = 'images/logo.svg', width = size, height = size, accessibilityHidden = true},
        ui.label{text = 'Online Store', font = 'title', wrap = false, style = Style.text('title', Style.type.heading)},
    }
end

-- The bag button with a badge that follows the count of the bag, kept current while its owner lives, a page or the frame.
function Shell.bagButton(page, id)
    local Bag = require('bag')
    local function show()
        local count = Bag.count()
        if page.gui and page.gui:has(id .. 'Count') then
            page.gui:set(id .. 'Count', {text = count, visible = count > 0})
        end
    end
    events.on('bagChanged', show, {owner = page})
    page.bagShows = page.bagShows or {}
    page.bagShows[#page.bagShows + 1] = show
    return ui.stack{id = id .. 'Box',
        ui.button{id = id, icon = Shell.icon('bag'), iconColor = 'text', variant = 'icon', tooltip = 'Bag', accessibilityLabel = 'Bag', onClick = Shell.openBag},
        ui.row{align = 'stretch', alignItems = 'start', padding = {4, 2, 0, 0}, ui.spacer{grow = 1}, ui.badge{id = id .. 'Count', text = '', tone = 'danger', solid = true, visible = false}},
    }
end

-- Shows the counts of the bag buttons once the GUI of the page is mounted.
function Shell.showBag(page)
    for _, show in ipairs(page.bagShows or {}) do
        show()
    end
end

-- A tab of the bar of a phone: the icon over the name, in the color of the text while it shows.
function Shell.barTab(tab)
    local selected = tab.id == Shell.tabs[1].id
    return ui.button{
        id = 'bar-' .. tab.id,
        text = tab.text,
        icon = Shell.barIcon(tab, selected),
        iconPlacement = 'top',
        variant = 'toolbar',
        grow = 1,
        height = Style.size.tabBar - Style.space.s,
        iconColor = selected and 'text' or 'textMuted',
        accessibilityLabel = tab.text,
        style = Shell.barStyle(selected),
        onClick = function()
            Shell.showTab(tab.id)
        end,
    }
end

function Shell.barIcon(tab, selected)
    return Shell.icon(tab.id == 'saved' and selected and 'heart-filled' or tab.icon)
end

function Shell.barStyle(selected)
    return {colors = {text = selected and 'text' or 'textMuted'}, fonts = {button = {size = Style.type.caption, bold = selected}}}
end

-- A tab of the navigation of the top: its name, underlined while it shows.
function Shell.navTab(tab)
    local selected = tab.id == Shell.tabs[1].id
    return ui.column{gap = 0, align = 'stretch',
        ui.button{id = 'nav-' .. tab.id, text = tab.text, variant = 'toolbar', grow = 1, accessibilityLabel = tab.text, style = Shell.navStyle(selected),
            onClick = function()
                Shell.showTab(tab.id)
            end},
        ui.divider{id = 'navLine-' .. tab.id, color = selected and 'text' or 'window', style = {metrics = {borderWidth = 3}}},
    }
end

function Shell.navStyle(selected)
    return {colors = {text = selected and 'text' or 'textMuted'}, fonts = {button = {size = Style.type.body, bold = selected}}}
end

-- The frame: the navigation of the top of a wide window with the wordmark, the tabs, the search and the bag, the middle where the stacks of the tabs show their screens, and the bar of tabs of a phone.
function Shell.frame()
    local space = Style.space
    local surface = {colors = {panel = 'window'}, metrics = {panelRadius = 0}}
    local nav = {id = 'navTabs', gap = space.xs, align = 'stretch'}
    local bar = {id = 'barTabs', gap = 0, height = Style.size.tabBar, padding = {0, space.xs}}
    for _, tab in ipairs(Shell.tabs) do
        nav[#nav + 1] = Shell.navTab(tab)
        bar[#bar + 1] = Shell.barTab(tab)
    end
    return ui.column{gap = 0,
        ui.panel{id = 'topNav', visible = false, gap = 0, padding = 0, style = surface,
            ui.row{gap = space.xs, height = Style.size.nav, padding = {0, space.xl}, align = 'stretch',
                ui.row{gap = 0, align = 'center', Shell.wordmark(48)},
                ui.spacer{grow = 1},
                ui.row(nav),
                ui.spacer{grow = 1},
                ui.row{gap = space.xs, align = 'center',
                    ui.button{id = 'navSearch', text = 'Search', icon = Shell.icon('search'), iconColor = 'textMuted', justify = 'start', width = 300, style = {colors = {text = 'textMuted'}, fonts = {button = {bold = false}}}, onClick = function()
                        Shell.showTab('browse', {focus = true})
                    end},
                    Shell.bagButton(Shell, 'navBag'),
                },
            },
            ui.divider{},
        },
        ui.column{id = 'middle', grow = 1, gap = 0, align = 'stretch'},
        ui.panel{id = 'tabBar', gap = 0, padding = 0, style = surface, ui.divider{}, ui.row(bar)},
    }
end

-- The page of a tab: the bar of a phone at the top, which the screen gives, over the content.
function Shell.tabPage(content, phoneBar)
    return ui.column{gap = 0, ui.column{id = 'phoneBar', gap = 0, phoneBar}, content}
end

-- Centers a node in the width it gets, as wide as it may be up to `maxWidth`. A `grow` lets the row take the free height of a column.
function Shell.centered(maxWidth, node, grow)
    node.grow = 100
    node.maxWidth = maxWidth
    node.align = 'stretch'
    return ui.row{gap = 0, grow = grow or 0, align = 'stretch', ui.spacer{grow = 1}, node, ui.spacer{grow = 1}}
end

-- The bar of a phone at the top of a tab: the wordmark, or a title, and the search and the bag.
function Shell.phoneBar(page, title)
    local space = Style.space
    return ui.column{gap = 0,
        ui.row{gap = space.xxs, height = Style.size.header, padding = {0, space.xs, 0, space.l},
            title and ui.label{text = title, font = 'title', grow = 1, wrap = false, style = Style.text('title', Style.type.heading)} or Shell.wordmark(44),
            title and ui.spacer{} or ui.spacer{grow = 1},
            ui.button{id = 'phoneSearch', icon = Shell.icon('search'), iconColor = 'text', variant = 'icon', tooltip = 'Search', accessibilityLabel = 'Search', onClick = function()
                Shell.showTab('browse', {focus = true})
            end},
            Shell.bagButton(page, 'phoneBag'),
        },
        ui.divider{},
    }
end

-- A bar at the top of a pushed screen: a back button, which goes back the way the system back does and shows while there is somewhere to go back to, a title in the serif and optional trailing nodes.
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
