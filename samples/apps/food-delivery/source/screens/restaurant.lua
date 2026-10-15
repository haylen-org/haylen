-- A restaurant: its cover, its menu in sections with sticky headers, the sheet of a dish with its size, extras and quantity, and the bar that leads to the cart.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Account = require('data.account')
local Basket = require('basket')
local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Restaurant = haylen.class('Restaurant', Page)

Restaurant.sections = {
    {id = 'popular', text = 'Popular'},
    {id = 'mains', text = 'Mains'},
    {id = 'sides', text = 'Sides'},
    {id = 'drinks', text = 'Drinks'},
    {id = 'desserts', text = 'Desserts'},
}

function Restaurant:enter()
    self.restaurant = Catalog.restaurant(self.params.restaurant)
    local restaurant = self.restaurant
    self.menu = {self:coverItem()}
    for _, item in ipairs(Catalog.menu(restaurant)) do
        self.menu[#self.menu + 1] = item
    end
    local jumps = {gap = 8, padding = {0, 20}}
    for _, section in ipairs(Restaurant.sections) do
        jumps[#jumps + 1] = ui.button{id = 'jump-' .. section.id, text = section.text, variant = 'toolbar', onClick = function()
            self:jumpTo(section.id)
        end}
    end
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(restaurant.name, {
            ui.button{id = 'favorite', icon = Shell.icon(self:isFavorite() and 'heart-filled' or 'heart'), iconColor = 'accentText', variant = 'icon', tooltip = 'Save', accessibilityLabel = 'Save to favorites', onClick = function()
                self:toggleFavorite()
            end},
            Widgets.cartButton(self),
        }),
        ui.scroll{axis = 'horizontal', scrollbar = false, ui.row(jumps)},
        ui.collection{id = 'menu', grow = 1, layout = 'grid', minCellSize = 560, gap = 4, padding = {8, 12, 24, 12}, rememberFocus = true,
            types = {
                cover = {template = self:coverTemplate(), span = 'full', interactive = false},
                section = {template = ui.panel{padding = {14, 8, 6, 8}, style = Shell.flatBar, ui.sectionTitle{part = 'title', bind = {text = 'title'}}}, span = 'full', sticky = true, interactive = false},
                dish = Widgets.dishRow(),
            },
            onSelect = function(event)
                if event.item ~= 'cover' then
                    self:showDish(Catalog.dish(event.item))
                end
            end,
        },
        ui.panel{id = 'checkoutBar', visible = false, padding = {10, 20, 12, 20}, style = Shell.flatBar,
            ui.row{gap = 0, ui.spacer{grow = 1}, ui.button{id = 'viewCart', text = '', variant = 'primary', grow = 100, maxWidth = 720, onClick = function()
                Shell.open(require('screens.cart'))
            end}, ui.spacer{grow = 1}},
        },
        self:dishDialog(),
        ui.dialog{id = 'newCart', title = 'Start a new cart?', message = 'Your cart holds dishes from another restaurant. A new cart takes their place.',
            buttons = {{id = 'keep', text = 'Keep my cart'}, {id = 'replace', text = 'New cart', variant = 'primary'}},
            onAnswer = function(event)
                if event.button == 'replace' then
                    Basket.clear()
                    self:addChosen()
                end
            end},
    })
    self.list = self.gui:collection('menu')
    self.list:setItems(self.menu)
    Widgets.showCount(self.gui)
    events.on('basketChanged', function()
        self:showBar()
    end, {owner = self})
    self:showBar()
end

function Restaurant:coverItem()
    local restaurant = self.restaurant
    return {
        id = 'cover',
        type = 'cover',
        image = restaurant.image,
        name = restaurant.name,
        details = restaurant.details,
        rating = string.format('%s · %s reviews', restaurant.ratingText, restaurant.reviews),
        place = string.format('%s · %.1f km away · %s', restaurant.neighborhood, restaurant.distance, string.rep('$', restaurant.priceLevel)),
        promo = restaurant.promo or '',
        hasPromo = restaurant.hasPromo,
    }
end

function Restaurant:coverTemplate()
    return ui.column{gap = 8, padding = {0, 8, 8, 8},
        ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', height = 230, align = 'stretch', maxWidth = 760, radius = 24},
        ui.row{gap = 10, margin = {8, 0, 0, 0},
            ui.label{part = 'name', bind = {text = 'name'}, font = 'title', grow = 1},
            ui.badge{part = 'promo', bind = {text = 'promo', visible = 'hasPromo'}, text = '', visible = false, tone = 'accent', solid = true},
        },
        ui.label{part = 'details', bind = {text = 'details'}, color = 'textMuted'},
        ui.row{gap = 6,
            ui.icon{image = Shell.icon('star'), size = 22, color = 'warning'},
            ui.label{part = 'rating', bind = {text = 'rating'}, font = 'caption'},
        },
        ui.row{gap = 6,
            ui.icon{image = Shell.icon('pin'), size = 22, color = 'textMuted'},
            ui.label{part = 'place', bind = {text = 'place'}, font = 'caption', color = 'textMuted'},
        },
    }
end

-- Scrolls the menu to the header of a section, which then sticks at the top.
function Restaurant:jumpTo(sectionId)
    self.list:scrollTo(self.restaurant.id .. '-' .. sectionId, {align = 'start'})
end

function Restaurant:isFavorite()
    return Account.favorites[self.restaurant.id] == true
end

function Restaurant:toggleFavorite()
    local favorite = not self:isFavorite()
    Account.favorites[self.restaurant.id] = favorite or nil
    self.gui:set('favorite', {icon = Shell.icon(favorite and 'heart-filled' or 'heart')})
    Shell.notify(favorite and 'Saved to your favorites.' or 'Removed from your favorites.')
end

-- Shows the bar that leads to the cart while the cart holds dishes of this restaurant.
function Restaurant:showBar()
    local visible = Basket.restaurant == self.restaurant.id
    self.gui:set('checkoutBar', {visible = visible})
    if visible then
        local count = Basket.count()
        self.gui:set('viewCart', {text = string.format('View cart · %d %s · %s', count, count == 1 and 'item' or 'items', Catalog.money(Basket.subtotal()))})
    end
end

-- The sheet of a dish: its picture and description, the size, the extras and the quantity.
function Restaurant:dishDialog()
    local extras = {gap = 0}
    for _, extra in ipairs(Catalog.extras) do
        extras[#extras + 1] = ui.checkbox{id = 'extra-' .. extra.id, text = extra.price > 0 and string.format('%s, %s', extra.text, Catalog.money(extra.price)) or extra.text, onChange = function(event)
            self.choice.extras[extra.id] = event.checked or nil
            self:showPrice()
        end}
    end
    local sizes = {}
    for _, size in ipairs(Catalog.sizes) do
        sizes[#sizes + 1] = {id = size.id, text = size.extra > 0 and string.format('%s, %s more', size.text, Catalog.money(size.extra)) or size.text}
    end
    return ui.dialog{id = 'dish', title = '', message = '',
        buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'add', text = 'Add', variant = 'primary'}},
        onAnswer = function(event)
            if event.button == 'add' then
                self:addChosen()
            end
        end,
        ui.image{id = 'dishImage', image = self.restaurant.image, fit = 'cover', height = 200, align = 'stretch', radius = 20},
        ui.sectionTitle{text = 'Size'},
        ui.radioGroup{id = 'size', items = sizes, selected = 'regular', onChange = function(event)
            for _, size in ipairs(Catalog.sizes) do
                if size.id == event.value then
                    self.choice.size = size
                end
            end
            self:showPrice()
        end},
        ui.sectionTitle{text = 'Extras'},
        ui.column(extras),
        ui.row{gap = 16, ui.label{text = 'Quantity', grow = 1}, ui.stepper{id = 'quantity', value = 1, min = 1, max = 20, width = 220, onChange = function(event)
            self.choice.quantity = event.value
            self:showPrice()
        end}},
    }
end

function Restaurant:showDish(dish)
    self.dish = dish
    self.choice = {size = Catalog.sizes[1], extras = {}, quantity = 1}
    local gui = self.gui
    gui:set('dishImage', {image = dish.image})
    gui:set('size', {selected = 'regular'})
    for _, extra in ipairs(Catalog.extras) do
        gui:set('extra-' .. extra.id, {checked = false})
    end
    gui:set('quantity', {value = 1})
    gui:set('dish', {open = true, title = dish.name, message = dish.description})
    self:showPrice()
end

-- The extras the person checked, in the order of the menu.
function Restaurant:chosenExtras()
    local extras = {}
    for _, extra in ipairs(Catalog.extras) do
        if self.choice.extras[extra.id] then
            extras[#extras + 1] = extra
        end
    end
    return extras
end

-- Writes the price of the choices on the add button of the sheet.
function Restaurant:showPrice()
    local unit = self.dish.price + self.choice.size.extra
    for _, extra in ipairs(self:chosenExtras()) do
        unit = unit + extra.price
    end
    self.gui:set('dish', {buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'add', text = 'Add · ' .. Catalog.money(unit * self.choice.quantity), variant = 'primary'}}})
end

function Restaurant:addChosen()
    local choice = {size = self.choice.size, extras = self:chosenExtras(), quantity = self.choice.quantity}
    if not Basket.add(self.dish, choice) then
        self.gui:set('newCart', {open = true})
        return
    end
    Shell.notify(string.format('%s added to your cart.', self.dish.name))
end

return Restaurant
