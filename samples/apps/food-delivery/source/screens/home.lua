-- The home tab: the delivery address, the search bar, the offer of the week, the cuisines and the shelves of restaurants.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Account = require('data.account')
local Catalog = require('data.catalog')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Home = haylen.class('Home', Page)

Home.shelfSize = 12
Home.cardWidth = 300

function Home:enter()
    self:mountGui(Shell.frame('home', ui.column{grow = 1, gap = 0,
        ui.row{gap = 12, padding = {10, 16, 6, 20},
            ui.column{gap = 0, grow = 1,
                ui.label{text = 'Deliver to', font = 'caption', color = 'textMuted'},
                ui.menuButton{id = 'address', text = self:addressText(), icon = Shell.icon('pin'), iconColor = 'accentText', variant = 'toolbar', justify = 'start', align = 'start', maxWidth = 560,
                    items = self:addressItems(), style = {metrics = {controlPaddingX = 4}},
                    onSelect = function(event)
                        Account.address = event.item
                        event.gui:set('address', {text = self:addressText()})
                    end},
            },
            Widgets.cartButton(self),
        },
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 1240, align = 'center', gap = 18, padding = {6, 20, 32, 20},
                ui.button{id = 'searchBar', text = 'Search dishes and restaurants', icon = Shell.icon('search'), iconColor = 'textMuted', justify = 'start', align = 'stretch', autofocus = true, onClick = function()
                    Shell.showTab('search', {focus = true})
                end},
                self:offer(),
                ui.sectionTitle{text = 'Cuisines'},
                ui.collection{id = 'cuisines', axis = 'horizontal', scrollbar = false, gap = 6, rememberFocus = true, types = {cuisine = {template = ui.column{gap = 8, width = 116, padding = {6, 4},
                    ui.image{part = 'image', bind = {image = 'image'}, fit = 'cover', width = 100, height = 100, radius = 50, align = 'center'},
                    ui.label{part = 'name', bind = {text = 'name'}, font = 'caption', textAlign = 'center', align = 'stretch', wrap = false},
                }}}, onSelect = function(event)
                    Shell.showTab('search', {cuisine = event.item})
                end},
                self:shelfTitle('Popular near you'),
                ui.collection{id = 'popular', axis = 'horizontal', scrollbar = false, gap = 16, rememberFocus = true, types = {restaurant = Widgets.restaurantCard(Home.cardWidth)}, onSelect = self.openRestaurant},
                self:shelfTitle('Offers today'),
                ui.collection{id = 'offers', axis = 'horizontal', scrollbar = false, gap = 16, rememberFocus = true, types = {restaurant = Widgets.restaurantCard(Home.cardWidth)}, onSelect = self.openRestaurant},
                ui.button{id = 'browse', text = string.format('Browse all %s restaurants', Home.thousands(Catalog.restaurantCount)), icon = Shell.icon('forward'), iconColor = 'text', align = 'center', onClick = function()
                    Shell.showTab('search')
                end},
            },
        },
    }))
    Widgets.showCount(self.gui)
    self.gui:collection('cuisines'):setItems(self:cuisineItems())
    self.gui:collection('popular'):setItems(self:shelf(function(restaurant) return restaurant.reviews > 1500 and restaurant.rating >= 4.6 end))
    self.gui:collection('offers'):setItems(self:shelf(function(restaurant) return restaurant.promo ~= nil end))
end

function Home:addressItems()
    local items = {}
    for _, address in ipairs(Account.addresses) do
        items[#items + 1] = {id = address.id, text = address.text .. ' · ' .. address.line, image = Shell.icon(address.icon)}
    end
    return items
end

function Home:addressText()
    local address = Account.currentAddress()
    return address.text .. ' · ' .. address.line
end

-- The card of the offer of the week, in the accent color with the illustration of the app.
function Home:offer()
    return ui.card{padding = {20, 12, 20, 26}, style = {colors = {raised = 'accent', border = 'accent'}},
        ui.row{gap = 8,
            ui.column{gap = 6, grow = 1,
                ui.label{text = 'Free delivery all week', font = 'heading', color = 'onAccent'},
                ui.label{text = 'On orders over $15 from hundreds of kitchens near you.', font = 'caption', color = 'onAccent'},
                ui.button{id = 'offer', text = 'Order now', align = 'start', margin = {8, 0, 0, 0}, style = {colors = {raised = '#FFFFFFFF', border = '#FFFFFFFF', text = 'accentStrong'}}, onClick = function()
                    Shell.showTab('search', {filter = 'free'})
                end},
            },
            ui.image{image = 'images/hero.png', width = 220, height = 165, fit = 'contain', accessibilityHidden = true},
        },
    }
end

function Home:shelfTitle(text)
    return ui.sectionTitle{text = text, margin = {8, 0, 0, 0}}
end

function Home:cuisineItems()
    local items = {}
    for _, cuisine in ipairs(Catalog.cuisines) do
        items[#items + 1] = {id = cuisine.id, type = 'cuisine', name = cuisine.name, image = 'dishes/' .. cuisine.art .. '.svg'}
    end
    return items
end

-- The first restaurants that pass a test, for a shelf.
function Home:shelf(test)
    local items = {}
    for _, restaurant in ipairs(Catalog.restaurants) do
        if test(restaurant) then
            items[#items + 1] = restaurant
            if #items == Home.shelfSize then
                break
            end
        end
    end
    return items
end

function Home.openRestaurant(event)
    Shell.open(require('screens.restaurant'), {restaurant = event.item})
end

function Home.thousands(number)
    local text = tostring(number)
    return text:reverse():gsub('(%d%d%d)', '%1,'):reverse():gsub('^,', '')
end

return Home
