-- The frame of the app: the tabs, the navigation between screens, the notices and the layout that follows the size of the window. Phones show a tab bar at the bottom, and wider windows show a rail on the side.
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Shell = {}

Shell.tabs = {
    {id = 'home', text = 'Home', icon = 'home', screen = 'screens.home'},
    {id = 'search', text = 'Search', icon = 'search', screen = 'screens.search'},
    {id = 'library', text = 'Library', icon = 'library', screen = 'screens.library'},
}
-- The width in UI units from which the window shows the rail instead of the tab bar.
Shell.wideWidth = 960
Shell.pushTransition = {duration = 0.3, effect = 'slideIn', direction = 'left', ease = 'cubicOut'}
Shell.backTransition = {duration = 0.26, effect = 'slideIn', direction = 'right', ease = 'cubicOut'}
Shell.riseTransition = {duration = 0.32, effect = 'moveIn', direction = 'up', ease = 'cubicOut'}
Shell.blankCover = 'images/logo.svg'
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

-- Pushes a screen over the current one with a slide, or with another transition.
function Shell.open(Screen, params, transition)
    window.setBackLeavesApp(false)
    return scene.push(Screen(params), transition or Shell.pushTransition)
end

-- Puts a screen in the place of the current one with the same slide, such as the route after the search.
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
        style = {colors = {text = selected and 'accentText' or 'textMuted'}},
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
            ui.label{text = 'Music Streaming', font = 'heading', wrap = false},
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
            Shell.playerBar(),
            ui.column{id = 'tabBar', gap = 0, ui.divider{}, ui.panel{padding = {6, 8, 4, 8}, style = Shell.flatBar, ui.row(bar)}},
        },
    }
end

-- The bar of the song that plays, above the tab bar of every screen: the cover and the names open the player, and the buttons play, pause and skip.
function Shell.playerBar()
    return ui.column{id = 'playerBar', visible = false, gap = 0,
        ui.progress{id = 'playerProgress', value = 0, style = {metrics = {progressHeight = 4}, colors = {track = '#00000000'}}},
        ui.panel{padding = {6, 12, 6, 12}, style = Shell.flatBar,
            ui.row{gap = 12,
                ui.imageButton{id = 'playerCover', image = Shell.blankCover, scale = 0.28, accessibilityLabel = 'Open the player', onClick = function()
                    Shell.openPlayer()
                end},
                ui.column{gap = 0, grow = 1,
                    ui.label{id = 'playerTitle', text = '', font = 'button', wrap = false},
                    ui.label{id = 'playerArtist', text = '', font = 'caption', color = 'textMuted', wrap = false},
                },
                ui.button{id = 'playerToggle', icon = Shell.icon('pause'), variant = 'icon', accessibilityLabel = 'Play or pause', onClick = function()
                    require('player').toggle()
                end},
                ui.button{id = 'playerNext', icon = Shell.icon('next'), variant = 'icon', accessibilityLabel = 'Next song', onClick = function()
                    require('player').next()
                end},
                ui.button{id = 'playerOpen', icon = Shell.icon('queue'), variant = 'icon', accessibilityLabel = 'Open the player', tooltip = 'Now playing', onClick = function()
                    Shell.openPlayer()
                end},
            },
        },
    }
end

function Shell.openPlayer()
    Shell.open(require('screens.now-playing'), nil, Shell.riseTransition)
end

-- Follows the player in the bar of a screen: the song and its state when they change, and the progress every frame.
function Shell.showPlayer(page)
    local gui = page.gui
    if not gui:has('playerBar') then
        return
    end
    local Player = require('player')
    local track = Player.current()
    local key = track and (track.id .. tostring(Player.playing) .. tostring(Player.loading)) or ''
    if page.playerKey ~= key then
        page.playerKey = key
        gui:set('playerBar', {visible = track ~= nil})
        if track then
            gui:set('playerCover', {image = track.cover})
            gui:set('playerTitle', {text = Player.loading and 'Making the song' or track.name})
            gui:set('playerArtist', {text = track.artistName})
            gui:set('playerToggle', {icon = Shell.icon(Player.playing and 'pause' or 'play')})
        end
    end
    if track then
        local value = math.floor(Player.elapsed() / track.length * 400) / 400
        if page.playerValue ~= value then
            page.playerValue = value
            gui:set('playerProgress', {value = value})
        end
    end
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
