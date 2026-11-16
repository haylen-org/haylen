-- The frame of the app: the tabs, the navigation between screens, the notices and the layout that follows the width of the window. Phones show a bar of tabs with their names at the bottom, and wide windows show the wordmark, the tabs, the search and the bag across the top, the way a store on the web does. The frame mounts once, and every screen lays out in its middle, so opening a screen changes only the middle and never builds the bars again.
local events = require('haylen.events')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Style = require('style')

local Shell = {}

Shell.tabs = {
    {id = 'home', text = 'Shop', icon = 'shop', screen = 'screens.shop'},
    {id = 'browse', text = 'Browse', icon = 'browse', screen = 'screens.browse'},
    {id = 'saved', text = 'Saved', icon = 'heart', screen = 'screens.saved'},
    {id = 'orders', text = 'Orders', icon = 'orders', screen = 'screens.orders'},
    {id = 'account', text = 'Account', icon = 'user', screen = 'screens.account'},
}
Shell.pushTransition = {duration = 0.3, effect = 'slideIn', direction = 'left', ease = 'cubicOut'}
Shell.backTransition = {duration = 0.26, effect = 'slideIn', direction = 'right', ease = 'cubicOut'}
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
    return Shell.width() >= Style.wideWidth
end

-- Mounts the frame and the notices, which stay above every screen, and shows the first tab. The bag button of the frame follows the bag while the app runs.
function Shell.start()
    Shell.gui = ui.mount(Shell.frame(), {layer = 1, sharedFocus = true})
    Shell.notices = ui.mount(ui.toast{id = 'notice', position = 'bottom', duration = 2.5, indicator = 'icon'}, {layer = 10, sharedFocus = true})
    Shell.showBag(Shell)
    Shell.adaptFrame()
    Shell.showTab('home')
end

-- The placement of the GUI of a screen: the middle of the frame, between the navigation of the top and the bar of tabs.
function Shell.middle()
    return {gui = Shell.gui, id = 'middle'}
end

function Shell.notify(text, tone)
    Shell.notices:command('notice', 'show', {text = text, tone = tone or 'neutral'})
end

-- Shows a tab as the only screen, without a transition, the way native tab bars switch. The parameters reach the screen of the tab.
function Shell.showTab(id, params)
    for _, tab in ipairs(Shell.tabs) do
        if tab.id == id then
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

-- Pushes a screen over the current one with a slide.
function Shell.open(Screen, params)
    window.setBackLeavesApp(false)
    return scene.push(Screen(params), Shell.pushTransition)
end

-- Puts a screen in the place of the current one with the same slide, such as the order after its confirmation.
function Shell.replace(Screen, params)
    return scene.replace(Screen(params), Shell.pushTransition)
end

-- Goes back one screen, or to the shop from the root of another tab.
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

-- Keeps the frame current, shows the bar of a phone at the top of a tab built with `Shell.tabPage`, and tells the screen when the window crosses the wide width.
function Shell.adapt(page)
    Shell.adaptFrame()
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

-- Marks the tab that shows, in the navigation of the top and in the bar of tabs.
function Shell.select(id)
    if Shell.current == id and Shell.gui:has('nav-' .. id) then
        return
    end
    Shell.current = id
    local nav, bar = {}, {}
    for _, tab in ipairs(Shell.tabs) do
        nav[#nav + 1] = Shell.navTab(tab, id)
        bar[#bar + 1] = Shell.barTab(tab, id)
    end
    Shell.gui:replaceChildren('navTabs', nav)
    Shell.gui:replaceChildren('barTabs', bar)
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
        ui.button{id = id, icon = Shell.icon('bag'), iconColor = 'text', variant = 'icon', tooltip = 'Bag', accessibilityLabel = 'Bag', onClick = function()
            Shell.open(require('screens.bag'))
        end},
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
function Shell.barTab(tab, current)
    local selected = tab.id == current
    return ui.button{
        id = 'bar-' .. tab.id,
        text = tab.text,
        icon = Shell.icon(tab.id == 'saved' and selected and 'heart-filled' or tab.icon),
        iconPlacement = 'top',
        variant = 'toolbar',
        grow = 1,
        height = Style.size.tabBar - Style.space.s,
        iconColor = selected and 'text' or 'textMuted',
        accessibilityLabel = tab.text,
        style = {colors = {text = selected and 'text' or 'textMuted'}, fonts = {button = {size = Style.type.caption, bold = selected}}},
        onClick = function()
            if not selected or scene.size() > 1 then
                Shell.showTab(tab.id)
            end
        end,
    }
end

-- A tab of the navigation of the top: its name, underlined while it shows.
function Shell.navTab(tab, current)
    local selected = tab.id == current
    return ui.column{gap = 0, align = 'stretch',
        ui.button{id = 'nav-' .. tab.id, text = tab.text, variant = 'toolbar', grow = 1, accessibilityLabel = tab.text,
            style = {colors = {text = selected and 'text' or 'textMuted'}, fonts = {button = {size = Style.type.body, bold = selected}}},
            onClick = function()
                if not selected or scene.size() > 1 then
                    Shell.showTab(tab.id)
                end
            end},
        ui.divider{color = selected and 'text' or 'window', style = {metrics = {borderWidth = 3}}},
    }
end

-- The frame: the navigation of the top of a wide window with the wordmark, the tabs, the search and the bag, the middle where the screens lay out, and the bar of tabs of a phone. The bars paint the paper of the store, so the screens that slide in the middle pass under them. The tabs arrive with `Shell.select`.
function Shell.frame()
    local space = Style.space
    local surface = {colors = {panel = 'window'}, metrics = {panelRadius = 0}}
    return ui.column{gap = 0, onCancel = Shell.back,
        ui.panel{id = 'topNav', visible = false, gap = 0, padding = 0, style = surface,
            ui.row{gap = space.xs, height = Style.size.nav, padding = {0, space.xl}, align = 'stretch',
                ui.row{gap = 0, align = 'center', Shell.wordmark(48)},
                ui.spacer{grow = 1},
                ui.row{id = 'navTabs', gap = space.xs, align = 'stretch'},
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
        ui.column{id = 'middle', grow = 1, gap = 0},
        ui.panel{id = 'tabBar', gap = 0, padding = 0, style = surface, ui.divider{}, ui.row{id = 'barTabs', gap = 0, height = Style.size.tabBar, padding = {0, space.xs}}},
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

-- A bar at the top of a pushed screen: a back button, a title in the serif and optional trailing nodes.
function Shell.header(title, trailing)
    local row = {gap = Style.space.xs, padding = {0, Style.space.s}, height = Style.size.header}
    row[#row + 1] = ui.button{id = 'back', icon = Shell.icon('back'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = Shell.back}
    row[#row + 1] = ui.label{id = 'title', text = title, font = 'heading', wrap = false, grow = 1}
    for _, node in ipairs(trailing or {}) do
        row[#row + 1] = node
    end
    return ui.column{gap = 0, ui.row(row), ui.divider{}}
end

return Shell
