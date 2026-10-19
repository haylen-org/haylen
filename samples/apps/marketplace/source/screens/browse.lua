-- The browse tab: every listing of the market in a recycled grid or as price pins on the city map, narrowed by the typed text, a category, a price range and the condition. The map is a play area, so the arrows, a gamepad or a remote pan it while it has the focus, and a strip of the listings around its center sits over it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local City = require('city')
local Market = require('data.market')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Browse = haylen.class('Browse', Page)

Browse.sorts = {
    {id = 'newest', text = 'Newest'},
    {id = 'cheap', text = 'Lowest price', less = function(a, b) return a.price < b.price end},
    {id = 'expensive', text = 'Highest price', less = function(a, b) return a.price > b.price end},
    {id = 'popular', text = 'Most viewed', less = function(a, b) return a.views > b.views end},
}
Browse.views = {{id = 'grid', text = 'Grid'}, {id = 'map', text = 'Map'}}
Browse.maxPrice = 600
Browse.pinLimit = 140
Browse.stripSize = 24
Browse.actions = {
    {name = 'mapPan', type = 'vector', up = {'key:up', 'button:dpadUp'}, down = {'key:down', 'button:dpadDown'}, left = {'key:left', 'button:dpadLeft'}, right = {'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
    {name = 'mapZoom', type = 'axis', positive = {'key:equal', 'axis:rightTrigger+'}, negative = {'key:minus', 'axis:leftTrigger+'}},
    {name = 'mapOpen', type = 'button', bindings = {'key:enter', 'key:space', 'button:south'}},
}

function Browse:load()
    self.font = assets.font('fonts/prompt-semibold.ttf')
end

function Browse:enter()
    for _, action in ipairs(Browse.actions) do
        input.defineAction(action)
    end
    self.query = ''
    self.category = self.params.category
    self.sort = Browse.sorts[1]
    self.low, self.high = 0, Browse.maxPrice
    self.conditions = {}
    self.view = 'grid'
    self.camera = graphics2d.newCamera()
    self.camera.position = {Market.city.width / 2, Market.city.height * 0.7}
    self.camera.zoom = {0.7, 0.7}
    local chips = {gap = 8, padding = {0, 20}, ui.chip{id = 'category-all', text = 'All', selected = self.category == nil, onChange = function()
        self:pickCategory(nil)
    end}}
    for _, category in ipairs(Market.categories) do
        chips[#chips + 1] = ui.chip{id = 'category-' .. category.id, text = category.name, selected = self.category == category.id, onChange = function()
            self:pickCategory(category.id)
        end}
    end
    local sorts = {}
    for _, sort in ipairs(Browse.sorts) do
        sorts[#sorts + 1] = {id = sort.id, text = sort.text}
    end
    self:mountGui(Shell.frame('home', ui.column{grow = 1, gap = 10, padding = {10, 0, 0, 0},
        ui.row{gap = 10, padding = {0, 16, 0, 20},
            ui.filterField{id = 'query', placeholder = 'Search the market', grow = 1, returnKey = 'search', onChange = function(event)
                self.query = string.lower(event.value)
                self:refresh()
            end},
            ui.button{id = 'filters', icon = Shell.icon('filter'), variant = 'icon', tooltip = 'Filters', accessibilityLabel = 'Filters', onClick = function()
                self:openFilters()
            end},
        },
        ui.scroll{axis = 'horizontal', scrollbar = false, ui.row(chips)},
        ui.row{gap = 10, padding = {0, 20},
            ui.label{id = 'count', text = '', font = 'caption', color = 'textMuted', grow = 1, wrap = false},
            ui.menuButton{id = 'sort', text = 'Newest', variant = 'toolbar', items = sorts, onSelect = function(event)
                for _, sort in ipairs(Browse.sorts) do
                    if sort.id == event.item then
                        self.sort = sort
                    end
                end
                event.gui:set('sort', {text = self.sort.text})
                self:refresh()
            end},
            ui.segmentedControl{id = 'view', items = Browse.views, selected = 'grid', width = 220, onChange = function(event)
                self:showView(event.value)
            end},
        },
        ui.stack{grow = 1, align = 'stretch',
            ui.collection{id = 'grid', align = 'stretch', layout = 'grid', minCellSize = 230, cellAspect = 1.42, gap = 14, padding = {4, 20, 24, 20}, rememberFocus = true,
                types = {listing = Widgets.listingCard()},
                onSelect = function(event)
                    Shell.open(require('screens.listing'), {listing = event.item})
                end,
                ui.emptyState{image = Shell.icon('search'), imageSize = 72, title = 'No listings match', message = 'Try other words, another category or a wider price range.'},
            },
            ui.column{id = 'mapLayer', visible = false, align = 'stretch', gap = 0,
                ui.playArea{id = 'map', grow = 1, accessibilityLabel = 'Map of the listings', onFocus = function()
                    self.gui:set('mapHint', {visible = true})
                end, onBlur = function()
                    self.gui:set('mapHint', {visible = false})
                end},
                ui.label{id = 'mapHint', visible = false, text = 'The arrows move the map, plus and minus zoom, and Enter opens the listing in the middle.', font = 'caption', color = 'textMuted', margin = {4, 20, 0, 20}},
                ui.collection{id = 'strip', axis = 'horizontal', scrollbar = false, gap = 12, padding = {8, 20, 16, 20}, rememberFocus = true,
                    types = {listing = Widgets.listingStrip()},
                    onSelect = function(event)
                        Shell.open(require('screens.listing'), {listing = event.item})
                    end,
                    onItemFocus = function(event)
                        self:centerOn(Market.listing(event.item))
                    end,
                },
            },
        },
        self:filterDialog(),
    }))
    self.grid = self.gui:collection('grid')
    self.strip = self.gui:collection('strip')
    self:refresh()
end

function Browse:filterDialog()
    local conditions = {gap = 0}
    for _, condition in ipairs(Market.conditions) do
        conditions[#conditions + 1] = ui.checkbox{id = 'condition-' .. condition, text = condition, onChange = function(event)
            self.pendingConditions[condition] = event.checked or nil
        end}
    end
    return ui.dialog{id = 'filterDialog', title = 'Filters', buttons = {{id = 'reset', text = 'Reset'}, {id = 'apply', text = 'Show results', variant = 'primary'}},
        onAnswer = function(event)
            if event.button == 'reset' then
                self.low, self.high, self.conditions = 0, Browse.maxPrice, {}
            else
                self.low, self.high, self.conditions = self.pendingLow, self.pendingHigh, self.pendingConditions
            end
            self:refresh()
        end,
        ui.sectionTitle{text = 'Price'},
        ui.rangeSlider{id = 'price', min = 0, max = Browse.maxPrice, low = 0, high = Browse.maxPrice, step = 10, showValue = true, decimals = 0, onChange = function(event)
            self.pendingLow, self.pendingHigh = event.low, event.high
        end},
        ui.sectionTitle{text = 'Condition'},
        ui.column(conditions),
    }
end

-- Opens the filters with the values in use, so a dialog closed without applying changes nothing.
function Browse:openFilters()
    self.pendingLow, self.pendingHigh, self.pendingConditions = self.low, self.high, {}
    for _, condition in ipairs(Market.conditions) do
        self.pendingConditions[condition] = self.conditions[condition]
        self.gui:set('condition-' .. condition, {checked = self.conditions[condition] == true})
    end
    self.gui:set('price', {low = self.low, high = self.high})
    self.gui:set('filterDialog', {open = true})
end

function Browse:pickCategory(id)
    local previous = self.category
    self.category = id
    self.gui:set('category-' .. (previous or 'all'), {selected = false})
    self.gui:set('category-' .. (id or 'all'), {selected = true})
    self:refresh()
end

-- Shows the listings that pass every filter in the chosen order, in the grid and on the map.
function Browse:refresh()
    local shown = {}
    local anyCondition = next(self.conditions) ~= nil
    for _, listing in ipairs(Market.listings) do
        if (not self.category or listing.category == self.category)
            and listing.price >= self.low and listing.price <= self.high
            and (not anyCondition or self.conditions[listing.condition])
            and (self.query == '' or string.find(listing.searchText, self.query, 1, true)) then
            shown[#shown + 1] = listing
        end
    end
    if self.sort.less then
        table.sort(shown, self.sort.less)
    end
    self.shown = shown
    self.grid:setItems(shown)
    self.gui:set('count', {text = string.format('%s listings', Browse.thousands(#shown))})
    self.stripCenter = nil
end

function Browse:showView(view)
    self.view = view
    self.gui:set('grid', {visible = view == 'grid'})
    self.gui:set('mapLayer', {visible = view == 'map'})
    self.stripCenter = nil
end

function Browse:update(dt)
    Page.update(self, dt)
    if self.view ~= 'map' then
        return
    end
    local camera = self.camera
    local panX, panY = input.vector('mapPan')
    local zoom = input.value('mapZoom')
    if ui.focusOwner() == 'playArea' then
        camera.position = {camera.x + panX * 600 * dt / camera.zoom.x, camera.y + panY * 600 * dt / camera.zoom.x}
        if zoom ~= 0 then
            self:zoomBy(1 + zoom * dt * 1.5)
        end
        if input.pressed('mapOpen') and self.nearest then
            Shell.open(require('screens.listing'), {listing = self.nearest.id})
        end
    end
    self:readPointer()
    self:fillStrip()
end

function Browse:zoomBy(factor)
    local zoom = math.max(0.3, math.min(2.5, self.camera.zoom.x * factor))
    self.camera.zoom = {zoom, zoom}
end

-- A drag on the map pans it, the wheel and a pinch zoom it, and a tap on a pin opens its listing. The taps come from the gestures of the engine, so a drag, a swipe or the fingers of a pinch never open a pin.
function Browse:readPointer()
    local area = self.gui:bounds('map')
    if not area then
        return
    end
    local camera = self.camera
    local function inside(x, y)
        return x >= area.x and y >= area.y and x <= area.x + area.width and y <= area.y + area.height
    end
    local touches = input.touches()
    local mouseX, mouseY = input.mousePosition()
    if #touches == 1 then
        local touch = touches[1]
        if touch.phase == 'began' then
            self.dragging = inside(touch.x, touch.y) and not ui.usingPointer()
        elseif self.dragging then
            camera.position = {camera.x - touch.dx / camera.zoom.x, camera.y - touch.dy / camera.zoom.x}
        end
    elseif #touches == 0 then
        if input.mousePressed('left') then
            self.dragging = inside(mouseX, mouseY) and not ui.usingPointer()
        elseif self.dragging and input.mouseDown('left') then
            local dx, dy = input.mouseDelta()
            camera.position = {camera.x - dx / camera.zoom.x, camera.y - dy / camera.zoom.x}
        else
            self.dragging = false
        end
    else
        self.dragging = false
    end
    local _, wheel = input.mouseScroll()
    if wheel ~= 0 and inside(mouseX, mouseY) and not ui.usingPointer() then
        self:zoomBy(wheel > 0 and 1.1 or 1 / 1.1)
    end
    for _, gesture in ipairs(input.gestures()) do
        if gesture.type == 'pinch' then
            self.pinchBase = self.pinchBase or camera.zoom.x
            local zoom = math.max(0.3, math.min(2.5, self.pinchBase * gesture.scale))
            camera.zoom = {zoom, zoom}
        elseif gesture.type == 'tap' and inside(gesture.x, gesture.y) and not ui.usingPointer() then
            self:tapAt(gesture.x, gesture.y)
        end
    end
    if #touches < 2 then
        self.pinchBase = nil
    end
end

function Browse:tapAt(x, y)
    for index = #self.pins, 1, -1 do
        local pin = self.pins[index]
        local screenX, screenY = self.camera:worldToScreen(pin.x, pin.y)
        if math.abs(screenX - x) < 44 and math.abs(screenY - y - 22) < 24 then
            Shell.open(require('screens.listing'), {listing = pin.id})
            return
        end
    end
end

-- Fills the strip with the listings nearest to the center of the map, again once the map moved far enough.
function Browse:fillStrip()
    local camera = self.camera
    local center = self.stripCenter
    if center and math.abs(center[1] - camera.x) + math.abs(center[2] - camera.y) < 120 / camera.zoom.x then
        return
    end
    self.stripCenter = {camera.x, camera.y}
    local nearest = {}
    for _, listing in ipairs(self.shown) do
        local distance = math.abs(listing.x - camera.x) + math.abs(listing.y - camera.y)
        if #nearest < Browse.stripSize or distance < nearest[#nearest].distance then
            local entry = {listing = listing, distance = distance}
            local place = #nearest + 1
            while place > 1 and nearest[place - 1].distance > distance do
                place = place - 1
            end
            table.insert(nearest, place, entry)
            nearest[Browse.stripSize + 1] = nil
        end
    end
    local items = {}
    for index, entry in ipairs(nearest) do
        items[index] = entry.listing
    end
    self.nearest = items[1]
    self.strip:setItems(items)
end

function Browse:centerOn(listing)
    self.camera.position = {listing.x, listing.y}
    self.focusedPin = listing
end

function Browse:render()
    Page.render(self)
    local area = self.view == 'map' and self.gui:bounds('map')
    if not area then
        return
    end
    local camera = self.camera
    camera.viewport = {area.x, area.y, area.width, area.height}
    graphics2d.beginWorld(camera)
    local dark = ui.theme() == 'marketDark'
    local visible = camera:visibleBounds()
    Market.city:draw(dark and City.dark or City.light, visible)
    self:drawPins(visible, dark)
end

-- Draws a price pin for every listing in view, up to a limit, with the listing in the middle of the map and the focused one in the accent color.
function Browse:drawPins(visible, dark)
    local unit = 1 / self.camera.zoom.x
    local accent = ui.themeColor('accent')
    local size = 20 * unit
    local pins, taken = self.pins or {}, {}
    local count = 0
    for _, listing in ipairs(self.shown) do
        if listing.x > visible.x and listing.x < visible.x + visible.width and listing.y > visible.y and listing.y < visible.y + visible.height then
            local active = listing == self.focusedPin or listing == self.nearest
            if active or self:claim(taken, listing, unit, size) then
                count = count + 1
                pins[count] = listing
                self:drawPin(listing, unit, size, active, accent, dark)
                if count == Browse.pinLimit then
                    break
                end
            end
        end
    end
    for index = count + 1, #pins do
        pins[index] = nil
    end
    self.pins = pins
end

-- Takes the cells of a coarse grid that a pin covers, or returns false when a pin drawn before it holds one of them, so prices never pile up and the hidden ones show as the map zooms in.
function Browse:claim(taken, listing, unit, size)
    local cell = 36 * unit
    local width = graphics2d.measureText(self.font, listing.priceText, {size = size}) + 22 * unit
    local left, right = math.floor((listing.x - width / 2) / cell), math.floor((listing.x + width / 2) / cell)
    local top, bottom = math.floor((listing.y - 40 * unit) / cell), math.floor(listing.y / cell)
    for row = top, bottom do
        for column = left, right do
            if taken[row * 100000 + column] then
                return false
            end
        end
    end
    for row = top, bottom do
        for column = left, right do
            taken[row * 100000 + column] = true
        end
    end
    return true
end

-- A price pin, the same size on the screen at every zoom.
function Browse:drawPin(listing, unit, size, active, accent, dark)
    local fill = active and accent or (dark and '#FF19231F' or '#FFFFFFFF')
    local width = graphics2d.measureText(self.font, listing.priceText, {size = size}) + 22 * unit
    local x, y = listing.x - width / 2, listing.y - 40 * unit
    graphics2d.drawShape({x, y, width, 32 * unit}, {radius = 16 * unit, color = fill, borderWidth = 2 * unit, borderColor = active and '#FFFFFFFF' or accent})
    graphics2d.drawPolygon({{listing.x - 6 * unit, y + 31 * unit}, {listing.x + 6 * unit, y + 31 * unit}, {listing.x, y + 40 * unit}}, fill)
    graphics2d.drawText(self.font, listing.priceText, listing.x, y + 16 * unit, {size = size, color = active and '#FFFFFFFF' or (dark and '#FFE7F0EC' or '#FF14211D'), anchor = {0.5, 0.5}})
end

function Browse.thousands(number)
    local text = tostring(number):reverse():gsub('(%d%d%d)', '%1,'):reverse():gsub('^,', '')
    return text
end

return Browse
