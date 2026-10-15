-- The search tab: every restaurant of the city in one recycled list, narrowed by the typed text, a cuisine and quick filters, in the order the person picks.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Search = haylen.class('Search', Page)

Search.filters = {
    {id = 'free', text = 'Free delivery', test = function(restaurant) return restaurant.fee == 0 end},
    {id = 'rated', text = 'Rated 4.5 and up', test = function(restaurant) return restaurant.rating >= 4.5 end},
    {id = 'fast', text = 'Under 30 minutes', test = function(restaurant) return restaurant.minutes < 30 end},
    {id = 'offers', text = 'Offers', test = function(restaurant) return restaurant.promo ~= nil end},
}

Search.sorts = {
    {id = 'recommended', text = 'Recommended'},
    {id = 'rating', text = 'Rating', less = function(a, b) return a.rating > b.rating end},
    {id = 'time', text = 'Delivery time', less = function(a, b) return a.minutes < b.minutes end},
    {id = 'fee', text = 'Delivery fee', less = function(a, b) return a.fee < b.fee end},
    {id = 'distance', text = 'Distance', less = function(a, b) return a.distance < b.distance end},
}

function Search:enter()
    self.query = ''
    self.cuisine = self.params.cuisine
    self.active = {}
    if self.params.filter then
        self.active[self.params.filter] = true
    end
    self.sort = Search.sorts[1]
    self:mountGui(Shell.frame('search', ui.column{grow = 1, gap = 10, padding = {10, 0, 0, 0},
        ui.row{gap = 12, padding = {0, 16, 0, 20},
            ui.filterField{id = 'query', placeholder = 'Search dishes and restaurants', grow = 1, returnKey = 'search', onChange = function(event)
                self.query = string.lower(event.value)
                self:refresh()
            end},
            Widgets.cartButton(self),
        },
        ui.scroll{axis = 'horizontal', scrollbar = false, ui.row{id = 'chips', gap = 8, padding = {0, 20}, children = self:chips()}},
        ui.row{gap = 8, padding = {0, 20},
            ui.label{id = 'count', text = '', font = 'caption', color = 'textMuted', grow = 1},
            ui.menuButton{id = 'sort', text = 'Sort by ' .. string.lower(self.sort.text), icon = Shell.icon('filter'), variant = 'toolbar', items = self:sortItems(), onSelect = function(event)
                for _, sort in ipairs(Search.sorts) do
                    if sort.id == event.item then
                        self.sort = sort
                    end
                end
                event.gui:set('sort', {text = 'Sort by ' .. string.lower(self.sort.text)})
                self:refresh()
            end},
        },
        ui.collection{id = 'results', grow = 1, layout = 'grid', minCellSize = 440, gap = 4, padding = {0, 12, 24, 12}, rememberFocus = true,
            types = {restaurant = Widgets.restaurantRow()},
            onSelect = function(event)
                Shell.open(require('screens.restaurant'), {restaurant = event.item})
            end,
            ui.emptyState{image = Shell.icon('search'), imageSize = 72, title = 'Nothing matches', message = 'Try another dish, a cuisine or fewer filters.'},
        },
    }))
    Widgets.showCount(self.gui)
    self.results = self.gui:collection('results')
    self:refresh()
    if self.params.focus then
        self.gui:command('query', 'focus')
    end
end

function Search:sortItems()
    local items = {}
    for _, sort in ipairs(Search.sorts) do
        items[#items + 1] = {id = sort.id, text = sort.text}
    end
    return items
end

-- The chips of the quick filters followed by one chip for every cuisine.
function Search:chips()
    local chips = {}
    for _, filter in ipairs(Search.filters) do
        chips[#chips + 1] = ui.chip{id = 'filter-' .. filter.id, text = filter.text, selected = self.active[filter.id] == true, onChange = function(event)
            self.active[filter.id] = event.selected or nil
            self:refresh()
        end}
    end
    for _, cuisine in ipairs(Catalog.cuisines) do
        chips[#chips + 1] = ui.chip{id = 'cuisine-' .. cuisine.id, text = cuisine.name, selected = self.cuisine == cuisine.id, onChange = function(event)
            if self.cuisine and self.cuisine ~= cuisine.id then
                event.gui:set('cuisine-' .. self.cuisine, {selected = false})
            end
            self.cuisine = event.selected and cuisine.id or nil
            self:refresh()
        end}
    end
    return chips
end

-- Shows the restaurants that pass the text, the cuisine and every active filter, in the chosen order. The list compares the new items by id, so the cells that stay keep their place.
function Search:refresh()
    local shown = {}
    for _, restaurant in ipairs(Catalog.restaurants) do
        if self:matches(restaurant) then
            shown[#shown + 1] = restaurant
        end
    end
    if self.sort.less then
        table.sort(shown, self.sort.less)
    end
    self.results:setItems(shown)
    self.gui:set('count', {text = #shown == 1 and '1 restaurant' or string.format('%d restaurants', #shown)})
end

function Search:matches(restaurant)
    if self.cuisine and restaurant.cuisine ~= self.cuisine then
        return false
    end
    if self.query ~= '' and not string.find(restaurant.searchText, self.query, 1, true) then
        return false
    end
    for _, filter in ipairs(Search.filters) do
        if self.active[filter.id] and not filter.test(restaurant) then
            return false
        end
    end
    return true
end

return Search
