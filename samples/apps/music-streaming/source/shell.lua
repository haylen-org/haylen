-- The frame of the app: the tabs, the routes of its links, the player that follows every screen, the messages and the layout that follows the width of the window. Phones show a bar of tabs at the bottom with a floating card of the song that plays above it, and wide windows show a sidebar with the playlists on the left and the player bar of a desktop across the bottom. The frame mounts once in the scene of the app, and the screens of each tab live in a stack of their own in its middle, so going to another screen changes only the middle while the bars stay. The player and the queue open over everything as presentations, a new flow with bars of its own.
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local system = require('haylen.system')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Style = require('style')

local Shell = {}

Shell.tabs = {
    {id = 'home', text = 'Home', icon = 'home', screen = 'screens.home'},
    {id = 'search', text = 'Search', icon = 'search', screen = 'screens.search'},
    {id = 'library', text = 'Library', icon = 'library', screen = 'screens.library'},
}
Shell.blankCover = 'images/logo.svg'

-- The scene of the app, which owns the frame and its navigation, paints the night behind them and keeps the frame current every frame.
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

-- Mounts the frame and the tabs in the middle of the frame, and starts the routes of the links over the tabs.
function Shell.scene:enter()
    Shell.gui = ui.mount(Shell.frame(), {owner = self, sharedFocus = true})
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
    Shell.showPlayer()
end

function Shell.scene:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), ui.themeColor('window'))
end

-- Confirms what the person just did in a toast at the top, clear of the player at the bottom.
function Shell.notify(text, tone)
    ui.showMessage{text = text, tone = tone or 'neutral', indicator = 'icon', position = 'top', owner = Shell.scene}
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

-- Shows a tab with the screens it kept, or goes back to its root when it already shows, the way native tab bars do.
function Shell.showTab(id)
    Shell.navigation:select(id)
end

-- Whether a presented screen, such as the player, still shows.
function Shell.isPresented(screen)
    return screen ~= nil and screen.presentation ~= nil and not screen.presentation.dismissed
end

-- The screen that the person sees: the player while it shows, or the top screen of the tab that shows.
function Shell.top()
    if Shell.isPresented(Shell.player) then
        return Shell.player
    end
    return Shell.navigation.current.top
end

-- Shows the screen of a location, such as "artist/12", in the stack of the tab that shows. A screen that already shows the same location further down the stack, such as the artist that its album opened from, comes back instead, so the stack never holds two copies of it.
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

-- Presents the player over everything through its route, a flow of its own with its close button, which goes back down with the system back.
function Shell.openPlayer()
    if Shell.isPresented(Shell.player) then
        return
    end
    Shell.router:push('player')
end

-- Presents the queue through its route as a sheet over the app or over the player, which rests at half the screen and opens to nearly all of it.
function Shell.openQueue()
    Shell.router:push('queue')
end

-- Shows the sidebar and the player bar on a wide window, and the bar of tabs with the floating card of the song otherwise.
function Shell.adaptFrame()
    local wide = Shell.isWide()
    if Shell.wide == wide then
        return
    end
    Shell.wide = wide
    Shell.gui:set('sidebar', {visible = wide})
    Shell.gui:set('phone', {visible = not wide})
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

-- Marks the tab that shows in the sidebar and in the bar of tabs, changing only the look of the two buttons.
function Shell.select(current, previous)
    for _, tab in ipairs(Shell.tabs) do
        if tab.id == current or tab.id == previous then
            local selected = tab.id == current
            local icon = Shell.icon(tab.icon .. (selected and '-filled' or ''))
            local color = selected and 'text' or 'textMuted'
            Shell.gui:set('bar-' .. tab.id, {icon = icon, iconColor = color, style = Shell.tabStyle(selected, 'bar')})
            Shell.gui:set('side-' .. tab.id, {icon = icon, iconColor = color, style = Shell.tabStyle(selected, 'side')})
        end
    end
end

-- The text of a tab is bright while it shows, in the caption size in the bar of a phone and in the body size with round corners in the sidebar.
function Shell.tabStyle(selected, placement)
    local colors = {text = selected and 'text' or 'textMuted'}
    if placement == 'bar' then
        return {colors = colors, fonts = {button = {size = Style.type.caption}}}
    end
    return {colors = colors, fonts = {button = {size = Style.type.body}}, metrics = {controlRadius = Style.radius.card}}
end

-- A tab of the bar of a phone: the icon over the name, filled and bright while it shows.
function Shell.barTab(tab)
    local selected = tab.id == Shell.tabs[1].id
    return ui.button{id = 'bar-' .. tab.id, text = tab.text, icon = Shell.icon(tab.icon .. (selected and '-filled' or '')), iconPlacement = 'top', variant = 'toolbar', grow = 1, height = Style.size.tabBar - Style.space.s,
        iconColor = selected and 'text' or 'textMuted', accessibilityLabel = tab.text, style = Shell.tabStyle(selected, 'bar'),
        onClick = function()
            Shell.showTab(tab.id)
        end}
end

-- A tab of the sidebar: the icon and the name, bright while it shows.
function Shell.sideTab(tab)
    local selected = tab.id == Shell.tabs[1].id
    return ui.button{id = 'side-' .. tab.id, text = tab.text, icon = Shell.icon(tab.icon .. (selected and '-filled' or '')), variant = 'toolbar', justify = 'start', align = 'stretch', height = 60,
        iconColor = selected and 'text' or 'textMuted', accessibilityLabel = tab.text, style = Shell.tabStyle(selected, 'side'),
        onClick = function()
            Shell.showTab(tab.id)
        end}
end

-- The wordmark: the logo and the name of the app.
function Shell.wordmark()
    return ui.row{gap = Style.space.s,
        ui.image{image = 'images/logo.svg', width = 44, height = 44, accessibilityHidden = true},
        ui.label{text = 'Music Streaming', font = 'button', wrap = false, style = Style.text('button', Style.type.heading * 0.8)},
    }
end

-- The items of the playlists of the person in the sidebar.
function Shell.playlistItems()
    local items = {}
    for index, playlist in ipairs(require('data.catalog').playlists) do
        items[index] = {id = playlist.id, text = playlist.name, caption = playlist.caption, image = playlist.cover, iconColor = false}
    end
    return items
end

-- Shows the playlists of the person in the sidebar again once one is made or deleted.
function Shell.showPlaylists()
    Shell.gui:set('sidePlaylists', {items = Shell.playlistItems()})
end

-- The sidebar of a wide window: the wordmark, the tabs and the playlists of the person.
function Shell.sidebar()
    local space = Style.space
    local tabs = {id = 'sideTabs', gap = space.xxs, align = 'stretch'}
    for _, tab in ipairs(Shell.tabs) do
        tabs[#tabs + 1] = Shell.sideTab(tab)
    end
    local column = {gap = space.xxs, padding = {space.m, 0, space.s, 0}, align = 'stretch',
        ui.row{padding = {0, space.s, space.l, space.s}, Shell.wordmark()},
        ui.column(tabs),
    }
    column[#column + 1] = ui.divider{margin = {space.s, space.s}}
    column[#column + 1] = ui.label{text = 'Your playlists', font = 'caption', color = 'textMuted', margin = {space.xs, space.s}}
    column[#column + 1] = ui.list{id = 'sidePlaylists', items = Shell.playlistItems(), onSelect = function(event)
        Shell.open(require('routes').location(event.item))
    end}
    return ui.row{id = 'sidebar', visible = false, gap = 0, align = 'stretch',
        ui.panel{width = Style.size.sidebar, padding = space.xs, align = 'stretch', style = {metrics = {panelRadius = 0}}, ui.scroll{grow = 1, height = 0, ui.column(column)}},
    }
end

-- The frame: the sidebar and the player bar of a wide window, or the floating card of the song and the bar of tabs of a phone, around the middle where the stacks of the tabs show their screens.
function Shell.frame()
    local space = Style.space
    local surface = {colors = {panel = 'window'}, metrics = {panelRadius = 0}}
    local bar = {id = 'barTabs', gap = 0, height = Style.size.tabBar, padding = {0, space.l}}
    for _, tab in ipairs(Shell.tabs) do
        bar[#bar + 1] = Shell.barTab(tab)
    end
    return ui.column{gap = 0,
        ui.row{grow = 1, gap = 0, align = 'stretch',
            Shell.sidebar(),
            ui.column{id = 'middle', grow = 1, gap = 0, align = 'stretch'},
        },
        ui.panel{id = 'desk', visible = false, gap = 0, padding = 0, style = surface, ui.divider{}, Shell.playerBar()},
        ui.panel{id = 'phone', gap = 0, padding = 0, style = surface, Shell.miniPlayer(), ui.row(bar)},
    }
end

-- The floating card of the song that plays on a phone: the cover and the names open the player, the buttons play, pause and skip, and a line shows how far the song went.
function Shell.miniPlayer()
    local space = Style.space
    return ui.column{id = 'mini', visible = false, gap = 0, padding = {space.xs, space.s, 0, space.s},
        ui.card{padding = space.xxs, gap = 0, style = {colors = {raised = Style.color.lifted, border = Style.color.lifted}, metrics = {panelRadius = Style.radius.card}},
            ui.row{gap = space.s, padding = space.xxs,
                ui.imageButton{id = 'miniCover', image = Shell.blankCover, scale = 0.3, accessibilityLabel = 'Open the player', onClick = Shell.openPlayer},
                ui.column{gap = 0, grow = 1,
                    ui.label{id = 'miniTitle', text = '', font = 'button', wrap = false},
                    ui.label{id = 'miniArtist', text = '', font = 'caption', color = 'textMuted', wrap = false},
                },
                ui.button{id = 'miniToggle', icon = Shell.icon('pause'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Play or pause', onClick = function()
                    require('player').toggle()
                end},
                ui.button{id = 'miniNext', icon = Shell.icon('next'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Next song', onClick = function()
                    require('player').next()
                end},
            },
            ui.progress{id = 'miniProgress', value = 0, margin = {0, space.s, space.xs, space.s}, style = {metrics = {progressHeight = 3}, colors = {track = '#33FFFFFF', accent = Style.color.text}}},
        },
    }
end

-- The player bar of a wide window: the song with its heart on the left, the controls and the position in the middle, and the queue and the volume on the right.
function Shell.playerBar()
    local space = Style.space
    local audio = require('haylen.audio')
    return ui.row{id = 'playerBar', gap = space.l, height = Style.size.playerBar, padding = {0, space.l},
        ui.row{gap = space.s, width = 380,
            ui.imageButton{id = 'barCover', image = Shell.blankCover, scale = 0.38, accessibilityLabel = 'Open the player', onClick = Shell.openPlayer},
            ui.column{gap = 0, grow = 1,
                ui.label{id = 'barTitle', text = 'Nothing plays', font = 'button', wrap = false},
                ui.label{id = 'barArtist', text = 'Pick a song to start', font = 'caption', color = 'textMuted', wrap = false},
            },
            ui.button{id = 'barLike', icon = Shell.icon('heart'), iconColor = 'textMuted', variant = 'icon', accessibilityLabel = 'Like', tooltip = 'Like', onClick = function()
                local track = require('player').current()
                if track then
                    require('data.catalog').toggleLike(track)
                end
            end},
        },
        ui.column{gap = 0, grow = 1, maxWidth = 720,
            ui.row{gap = space.s, justify = 'center',
                ui.button{id = 'barShuffle', icon = Shell.icon('shuffle'), iconColor = 'textMuted', variant = 'icon', accessibilityLabel = 'Shuffle', tooltip = 'Shuffle', onClick = function()
                    local Player = require('player')
                    Player.setShuffle(not Player.shuffle)
                end},
                ui.button{id = 'barPrevious', icon = Shell.icon('previous'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Previous song', onClick = function()
                    require('player').previous()
                end},
                ui.button{id = 'barToggle', icon = Shell.icon('play'), iconColor = 'onAccent', variant = 'primary', width = 56, height = 56, accessibilityLabel = 'Play or pause', style = {colors = Shell.lightColors, metrics = {controlRadius = 28, controlPaddingX = 0, iconSize = 26}}, onClick = function()
                    require('player').toggle()
                end},
                ui.button{id = 'barNext', icon = Shell.icon('next'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Next song', onClick = function()
                    require('player').next()
                end},
                ui.button{id = 'barRepeat', icon = Shell.icon('repeat'), iconColor = 'textMuted', variant = 'icon', accessibilityLabel = 'Repeat', tooltip = 'Repeat', onClick = function()
                    require('player').cycleRepeat()
                end},
            },
            ui.row{gap = space.s,
                ui.label{id = 'barElapsed', text = '0:00', font = 'caption', color = 'textMuted', width = 56, textAlign = 'end'},
                ui.slider{id = 'barPosition', value = 0, grow = 1, accessibilityLabel = 'Position in the song', onPress = function()
                    Shell.scrubbing = true
                end, onRelease = function()
                    Shell.scrubbing = false
                end, onChange = function(event)
                    local Player = require('player')
                    local track = Player.current()
                    if track then
                        Player.seek(event.value * track.length)
                    end
                end},
                ui.label{id = 'barLength', text = '0:00', font = 'caption', color = 'textMuted', width = 56},
            },
        },
        ui.row{gap = space.xs, width = 300, justify = 'end',
            ui.button{id = 'barQueue', icon = Shell.icon('queue'), iconColor = 'textMuted', variant = 'icon', accessibilityLabel = 'Queue', tooltip = 'Queue', onClick = Shell.openQueue},
            ui.icon{image = Shell.icon('volume'), size = 26, color = 'textMuted'},
            ui.slider{id = 'barVolume', value = audio.busVolume('music'), width = 150, accessibilityLabel = 'Volume', onChange = function(event)
                audio.setBusVolume('music', event.value)
            end},
        },
    }
end

-- The colors of the round buttons of the player in the color of the text, with the icon in the color of the night.
Shell.lightColors = {accent = Style.color.text, accentHover = '#FFFFFFFF', accentStrong = '#FFD9D5EA', onAccent = Style.color.night}

-- Follows the player in the card of a phone or the bar of a wide window: the song and its state when they change, and the position every frame.
function Shell.showPlayer()
    local gui = Shell.gui
    local Player = require('player')
    local Catalog = require('data.catalog')
    local track = Player.current()
    local layout = tostring(Shell.wide)
    local key = track and (track.id .. tostring(Player.playing) .. tostring(Player.loading) .. tostring(track.liked) .. tostring(Player.shuffle) .. Player.repeatMode .. layout) or layout
    if Shell.playerShown ~= key then
        Shell.playerShown = key
        gui:set('mini', {visible = track ~= nil})
        gui:set('desk', {visible = Shell.wide})
        if track then
            local name = Player.loading and 'Making the song' or track.name
            local icon = Shell.icon(Player.playing and 'pause' or 'play')
            gui:set('miniCover', {image = track.cover})
            gui:set('miniTitle', {text = name})
            gui:set('miniArtist', {text = track.artistName})
            gui:set('miniToggle', {icon = icon})
            gui:set('barCover', {image = track.cover})
            gui:set('barTitle', {text = name})
            gui:set('barArtist', {text = track.artistName})
            gui:set('barToggle', {icon = icon})
            gui:set('barLength', {text = track.durationText})
            gui:set('barLike', {icon = Shell.icon(track.liked and 'heart-filled' or 'heart'), iconColor = track.liked and 'accentText' or 'textMuted'})
            gui:set('barShuffle', {iconColor = Player.shuffle and 'accentText' or 'textMuted'})
            gui:set('barRepeat', {iconColor = Player.repeatMode ~= 'off' and 'accentText' or 'textMuted'})
        end
    end
    if not track then
        return
    end
    local elapsed = Player.elapsed()
    local value = math.floor(elapsed / track.length * 400) / 400
    if Shell.playerValue ~= value then
        Shell.playerValue = value
        gui:set('miniProgress', {value = value})
        if not Shell.scrubbing then
            gui:set('barPosition', {value = value})
        end
    end
    local second = math.floor(elapsed)
    if Shell.playerSecond ~= second then
        Shell.playerSecond = second
        gui:set('barElapsed', {text = Catalog.duration(second)})
    end
end

-- Centers a node in the width it gets, as wide as it may be up to `maxWidth`. A `grow` lets the row take the free height of a column.
function Shell.centered(maxWidth, node, grow)
    node.grow = 100
    node.maxWidth = maxWidth
    node.align = 'stretch'
    return ui.row{gap = 0, grow = grow or 0, align = 'stretch', ui.spacer{grow = 1}, node, ui.spacer{grow = 1}}
end

-- A bar at the top of a pushed screen: a back button, a title and optional trailing nodes. The back button goes back the way the system back does.
function Shell.header(title, trailing)
    local row = {gap = Style.space.xs, padding = {0, Style.space.s}, height = Style.size.header}
    row[#row + 1] = ui.button{id = 'back', icon = Shell.icon('back'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Back', tooltip = 'Back', onClick = ui.goBack}
    row[#row + 1] = ui.label{id = 'title', text = title, font = 'heading', wrap = false, grow = 1}
    for _, node in ipairs(trailing or {}) do
        row[#row + 1] = node
    end
    return ui.row(row)
end

-- A bar at the top of a sheet: a title and optional trailing nodes, with the button that closes the sheet the way the system back does.
function Shell.sheetHeader(title, trailing)
    local row = {gap = Style.space.xs, padding = {0, Style.space.s}, height = Style.size.header}
    row[#row + 1] = ui.button{id = 'close', icon = Shell.icon('down'), iconColor = 'text', variant = 'icon', accessibilityLabel = 'Close', tooltip = 'Close', onClick = ui.goBack}
    row[#row + 1] = ui.label{id = 'title', text = title, font = 'heading', wrap = false, grow = 1}
    for _, node in ipairs(trailing or {}) do
        row[#row + 1] = node
    end
    return ui.row(row)
end

return Shell
