-- The dashboard: one screen of a port in operation, with live metric cards, charts, long lists of vessels, containers and events and a table of berths. The cards change through "gui:set" only when a shown value changes, the lists bind only the items in view, and the panel raises and lowers the load: the cards, the items in the lists and how often the values change.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local jobs = require('haylen.jobs')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Charts = require('charts')
local Hud = require('hud')
local Port = require('port')
local Tour = require('tour')

local Dashboard = haylen.class('Dashboard', scene.Scene)

Dashboard.cardCounts = {{id = '6', text = '6'}, {id = '12', text = '12'}, {id = '24', text = '24'}, {id = '48', text = '48'}}
Dashboard.itemCounts = {{id = '1000', text = '1,000'}, {id = '10000', text = '10,000'}, {id = '100000', text = '100,000'}}
Dashboard.rates = {{id = 'frame', text = 'Every frame'}, {id = 'tenth', text = 'Ten a second'}, {id = 'paused', text = 'Paused'}}
Dashboard.rateNames = {frame = 'every frame', tenth = 'ten times a second', paused = 'not at all'}
Dashboard.startCards = 12
Dashboard.startItems = 10000
Dashboard.tenth = 0.1
Dashboard.historySize = 180
Dashboard.feedSize = 300
Dashboard.feedInterval = 0.3
Dashboard.tableInterval = 0.25
Dashboard.containerChanges = 4
-- The lists keep their rows clear of the scrollbar on their end edge.
Dashboard.listPadding = {0, 18, 0, 0}
-- A sticky header paints the color of its card, so the rows that scroll under it never show through.
Dashboard.headerStyle = {colors = {panel = '#FF1B2430'}}
Dashboard.tableColumns = {{text = 'Berth', width = 60, align = 'center'}, {text = 'Vessel'}, {text = 'Done', width = 60, align = 'end'}, {text = 'State', width = 100}}
Dashboard.actions = {actions = {
    {name = 'moreCards', type = 'button', bindings = {'key:equal', 'key:keypadAdd', 'axis:rightTrigger+'}},
    {name = 'fewerCards', type = 'button', bindings = {'key:minus', 'key:keypadSubtract', 'axis:leftTrigger+'}},
    {name = 'rate', type = 'button', bindings = {'key:r', 'button:rightStick'}},
}}

function Dashboard:load(context)
    context:progress(0, 'Drawing the panels')
    local texture, failure = assets.vectorImage('dashboard/panel.svg'):rasterize(2):await()
    if not texture then
        error(failure, 0)
    end
    self.panelTexture = texture
    self.icons = {
        throughput = assets.vectorImage('dashboard/icon_moves.svg'),
        berthChart = assets.vectorImage('dashboard/icon_berths.svg'),
        cranes = assets.vectorImage('dashboard/icon_share.svg'),
    }
end

local function chartNode(id, title, grow)
    return ui.column{id = id, grow = grow, align = 'stretch', padding = {14, 16, 0, 50}, ui.label{text = title, font = 'button', wrap = false}}
end

local function section(title, grow, child)
    return ui.card{grow = grow, align = 'stretch', gap = 8, ui.label{text = title, font = 'button'}, child}
end

function Dashboard:metricCard(metric)
    local id = metric.id
    return ui.card{gap = 4,
        ui.row{gap = 8,
            ui.label{text = metric.title, font = 'caption', color = 'textMuted', grow = 1, wrap = false},
            ui.badge{id = id .. '-badge', text = 'Normal', tone = 'success'},
        },
        ui.label{id = id .. '-value', text = '0', font = 'heading', wrap = false},
        ui.label{text = metric.kind.unit, font = 'caption', color = 'textMuted', wrap = false},
        ui.progress{id = id .. '-bar', value = 0},
    }
end

function Dashboard:content()
    return ui.column{grow = 1, align = 'stretch', gap = 12,
        ui.row{gap = 14, height = 56,
            ui.image{image = 'dashboard/logo.svg', width = 52, height = 52},
            ui.column{gap = 0, grow = 1,
                ui.label{text = 'Port Operations', font = 'title', wrap = false},
                ui.label{text = 'A shift at a made-up container port, live.', font = 'caption', color = 'textMuted', wrap = false},
            },
            ui.badge{id = 'alerts', text = 'No alerts', tone = 'success', solid = true},
            ui.label{id = 'clock', text = '06:00', font = 'heading'},
        },
        ui.scroll{id = 'cardScroll', height = 268, ui.grid{id = 'cards', columns = 6, gap = 10}},
        ui.row{gap = 12, height = 250, align = 'stretch',
            chartNode('throughput', 'Crane moves per hour', 2),
            chartNode('berthChart', 'Berths at work', 1.6),
            chartNode('cranes', 'Share of the cranes', 1),
        },
        ui.row{gap = 12, grow = 1, align = 'stretch',
            section('Vessels', 2.6, ui.collection{id = 'vessels', grow = 1, gap = 6, padding = Dashboard.listPadding, selection = 'single', types = {
                terminal = {template = ui.panel{padding = {4, 0}, style = Dashboard.headerStyle, ui.label{part = 'title', bind = {text = 'title'}, font = 'button', color = 'accentText'}}, sticky = true, interactive = false},
                vessel = {template = ui.column{gap = 4,
                    ui.row{gap = 8, ui.label{part = 'name', bind = {text = 'name'}, grow = 1, wrap = false}, ui.badge{part = 'status', bind = {text = 'status', tone = 'tone'}}},
                    ui.row{gap = 8, ui.progress{part = 'progress', bind = {value = 'progress'}, grow = 1}, ui.label{part = 'eta', bind = {text = 'eta'}, font = 'caption', color = 'textMuted', wrap = false}},
                }},
            }, ui.busyIndicator{size = 40}}),
            section('Containers', 3, ui.collection{id = 'containers', grow = 1, layout = 'grid', minCellSize = 130, cellAspect = 0.62, gap = 8, padding = Dashboard.listPadding, selection = 'multiple', types = {
                box = {template = ui.column{gap = 2, padding = 6,
                    ui.label{part = 'code', bind = {text = 'code'}, wrap = false},
                    ui.label{part = 'block', bind = {text = 'block'}, font = 'caption', color = 'textMuted', wrap = false},
                    ui.badge{part = 'status', bind = {text = 'status', tone = 'tone'}},
                }},
            }, ui.busyIndicator{size = 40}}),
            section('Berths', 2.8, ui.scroll{grow = 1, height = 0, ui.table{id = 'berths', columns = Dashboard.tableColumns, rows = self.port:stepBerths(0)}}),
            section('Events', 2.6, ui.collection{id = 'feed', grow = 1, gap = 4, padding = Dashboard.listPadding, stickToEnd = true, types = {
                event = {template = ui.row{gap = 8,
                    ui.statusIndicator{part = 'line', bind = {text = 'text', tone = 'tone'}, grow = 1},
                    ui.label{part = 'time', bind = {text = 'time'}, font = 'caption', color = 'textMuted', wrap = false},
                }, interactive = false},
            }}),
        },
    }
end

function Dashboard:enter()
    input.loadActions(Dashboard.actions)
    self.port = Port.new(17)
    self.rate = 'frame'
    self.sinceTick = 0
    self.sinceTable = 0
    self.sinceEvent = 0
    self.history = collections.newRingBuffer(Dashboard.historySize)
    self.events = {}
    self.slice = graphics2d.newNineSlice(self.panelTexture, {borders = {28, 28, 28, 28}})
    self.hud = Hud.new(self, {
        content = self:content(),
        title = 'Busy Interface',
        caption = 'Every value of the cards changes through "gui:set" only when what it shows changes.',
        rows = {
            {id = 'changes', label = 'Changes per second'},
            {id = 'items', label = 'Items in the lists'},
            {id = 'cells', label = 'Cells alive'},
            {id = 'memory', label = 'Lua memory'},
        },
        controls = {
            ui.label{text = 'Live cards', font = 'caption', color = 'textMuted'},
            ui.segmentedControl{id = 'cardCount', items = Dashboard.cardCounts, selected = tostring(Dashboard.startCards), onChange = function(event) self:showCards(tonumber(event.value)) end},
            ui.label{text = 'Items in each list', font = 'caption', color = 'textMuted'},
            ui.segmentedControl{id = 'itemCount', items = Dashboard.itemCounts, selected = tostring(Dashboard.startItems), onChange = function(event) self:fillLists(tonumber(event.value)) end},
            ui.label{text = 'Values change', font = 'caption', color = 'textMuted'},
            ui.segmentedControl{id = 'changeRate', items = Dashboard.rates, selected = 'frame', onChange = function(event) self:setRate(event.value) end},
            ui.toggle{id = 'charts', text = 'Charts', checked = true, onChange = function(event) self.showCharts = event.checked end},
        },
        hint = 'Plus and minus or the triggers change the live cards, and R or a click of the right stick changes how often the values change. The arrows, a gamepad or a remote move between the controls, the lists and the table, and the wheel, the scrollbars or a finger scroll the lists.',
        focus = 'cardCount',
    })
    self.gui = self.hud.gui
    self.showCharts = true
    self.vesselList = self.gui:collection('vessels')
    self.containerList = self.gui:collection('containers')
    self.feed = self.gui:collection('feed')
    self.feed:setItems(self.events)
    self:showCards(Dashboard.startCards)
    self:fillLists(Dashboard.startItems)
    if haylen.platform == 'headless' then
        Tour.start(self, self:tourSteps())
    end
end

-- The steps of the automatic run: more and fewer cards, longer lists, every rate and the charts off.
function Dashboard:tourSteps()
    local function step(change)
        return function()
            change()
            return string.format('%d cards, %s items in each list, values changing %s', #self.metrics, Hud.count(self.itemCount), Dashboard.rateNames[self.rate])
        end
    end
    return {
        step(function() end),
        step(function() self:showCards(48) end),
        step(function() self:fillLists(100000) end),
        step(function() self.vesselList:scrollTo(math.max(1, self.vesselList.count // 2), {align = 'center', animated = false}) end),
        step(function() self:setRate('tenth') end),
        step(function() self:setRate('paused') end),
        step(function() self.showCharts = false end),
        step(function() self:showCards(6) end),
        step(function() self:fillLists(1000) end),
    }
end

-- Rebuilds the grid of cards with `count` metrics.
function Dashboard:showCards(count)
    self.metrics = self.port:metrics(count)
    local cards = {}
    for index, metric in ipairs(self.metrics) do
        cards[index] = self:metricCard(metric)
    end
    self.gui:replaceChildren('cards', cards)
    self.hud:forget()
    self.hud:set('cardCount', 'selected', tostring(count))
    self:showMetrics()
end

-- Builds the vessels and the containers of the lists in jobs, so even a hundred thousand of each never stalls a frame.
function Dashboard:fillLists(count)
    self.itemCount = count
    self.hud:set('itemCount', 'selected', tostring(count))
    local generation = (self.listGeneration or 0) + 1
    self.listGeneration = generation
    self:spawn(function()
        local vessels, failure = jobs.spawn(self.port.vessels, self.port, count):await()
        if not vessels then
            error(failure, 0)
        end
        local containers, problem = jobs.spawn(self.port.containers, self.port, count):await()
        if not containers then
            error(problem, 0)
        end
        if generation == self.listGeneration then
            self.vessels, self.containers = vessels, containers
            self.vesselList:setItems(vessels)
            self.containerList:setItems(containers)
        end
    end)
end

function Dashboard:setRate(rate)
    self.rate = rate
    self.hud:set('changeRate', 'selected', rate)
end

function Dashboard:nextRate()
    for index, rate in ipairs(Dashboard.rates) do
        if rate.id == self.rate then
            self:setRate(Dashboard.rates[index % #Dashboard.rates + 1].id)
            return
        end
    end
end

function Dashboard:nextCards(direction)
    for index, option in ipairs(Dashboard.cardCounts) do
        if tonumber(option.id) == #self.metrics then
            local next = Dashboard.cardCounts[index + direction]
            if next then
                self:showCards(tonumber(next.id))
            end
            return
        end
    end
end

-- Shows the value, the bar and the badge of every card, each through "gui:set" only when it changed.
function Dashboard:showMetrics()
    local hud = self.hud
    local alerts = 0
    for _, metric in ipairs(self.metrics) do
        local kind = metric.kind
        local share = (metric.value - kind.low) / (kind.high - kind.low)
        local tone = share > 0.85 and 'danger' or share > 0.6 and 'warning' or 'success'
        if tone == 'danger' then
            alerts = alerts + 1
        end
        hud:show(metric.id .. '-value', string.format('%.0f', metric.value))
        hud:set(metric.id .. '-bar', 'value', math.floor(share * 100 + 0.5) / 100)
        if hud:set(metric.id .. '-badge', 'tone', tone) then
            hud:show(metric.id .. '-badge', tone == 'danger' and 'Alert' or tone == 'warning' and 'Busy' or 'Normal')
            hud:set(metric.id .. '-bar', 'tone', tone == 'success' and 'accent' or tone)
        end
    end
    hud:show('alerts', alerts == 0 and 'No alerts' or alerts == 1 and 'One alert' or alerts .. ' alerts')
    hud:set('alerts', 'tone', alerts == 0 and 'success' or 'danger')
end

-- Moves the vessels and the containers in view and binds them again, which costs only the rows that show.
function Dashboard:advanceLists(dt)
    if not self.vessels then
        return
    end
    local changed = {}
    local first, last = self.vesselList:visibleRange()
    if first then
        self.port:stepVessels(self.vessels, first, last, dt, changed)
        if #changed > 0 then
            self.vesselList:reload(changed)
        end
    end
    first, last = self.containerList:visibleRange()
    if first then
        changed = {}
        self.port:stepContainers(self.containers, first, last, Dashboard.containerChanges, changed)
        if #changed > 0 then
            self.containerList:reload(changed)
        end
    end
end

function Dashboard:craneMoves()
    local total, cranes = 0, {}
    for _, metric in ipairs(self.metrics) do
        if metric.kind == Port.metricKinds[1] then
            total = total + metric.value
            cranes[#cranes + 1] = metric.value
        end
    end
    return total, cranes
end

-- Advances the simulation by `dt` and shows it: the cards, the lists in view and the history of the line chart.
function Dashboard:tick(dt)
    self.port:stepMetrics(self.metrics, dt)
    self:showMetrics()
    self:advanceLists(dt)
    self.history:push((self:craneMoves()))
end

function Dashboard:update(dt)
    if input.pressed('moreCards') then
        self:nextCards(1)
    end
    if input.pressed('fewerCards') then
        self:nextCards(-1)
    end
    if input.pressed('rate') then
        self:nextRate()
    end

    if self.rate ~= 'paused' then
        debugging.beginScope('liveValues')
        self.sinceTick = self.sinceTick + dt
        if self.rate == 'frame' or self.sinceTick >= Dashboard.tenth then
            self:tick(self.sinceTick)
            self.sinceTick = 0
        end
        self.sinceTable = self.sinceTable + dt
        if self.sinceTable >= Dashboard.tableInterval then
            self.gui:set('berths', {rows = self.port:stepBerths(self.sinceTable)})
            self.hud:show('clock', Port.clockText(self.port.clock))
            self.sinceTable = 0
        end
        self.sinceEvent = self.sinceEvent + dt
        if self.sinceEvent >= Dashboard.feedInterval then
            self.sinceEvent = 0
            self.feed:insert(#self.events + 1, {self.port:nextEvent()})
            if #self.events > Dashboard.feedSize then
                self.feed:remove(1, #self.events - Dashboard.feedSize)
            end
        end
        debugging.endScope()
    end

    if self.hud:update(dt) then
        local cells = self.hud.stats.objects.UiCell
        self.hud:show('changes', Hud.count(self.hud:takeChanges() / Hud.interval))
        self.hud:show('items', Hud.count(self.vessels and #self.vessels + #self.containers + #self.events or 0))
        self.hud:show('cells', Hud.count(cells and cells.alive or 0))
        self.hud:show('memory', string.format('%.1f MB', self.hud.stats.memory.lua / 1048576))
    end
end

function Dashboard:renderUi()
    graphics2d.beginScreen()
    if not self.showCharts then
        return
    end
    for id, icon in pairs(self.icons) do
        local rect = self.gui:bounds(id)
        if rect then
            Charts.panel(self.slice, rect)
            graphics2d.drawVector(icon, rect.x + 16, rect.y + 14, {width = 24, height = 24, pivotX = 0, pivotY = 0, color = Charts.line, layer = 2})
        end
    end
    local throughput = self.gui:bounds('throughput')
    if throughput then
        local _, cranes = self:craneMoves()
        Charts.lineChart(throughput, self.history:values(), #cranes * 24)
    end
    local berths = self.gui:bounds('berthChart')
    if berths then
        local values, tones, names = {}, {}, {}
        for index, berth in ipairs(self.port.berths) do
            values[index], tones[index], names[index] = berth.progress, Port.stages[berth.stage].tone, tostring(index)
        end
        Charts.barChart(berths, values, tones, names)
    end
    local share = self.gui:bounds('cranes')
    if share then
        local total, cranes = self:craneMoves()
        Charts.ringChart(share, cranes, string.format('%.0f', total), 'Per hour')
    end
end

return Dashboard
