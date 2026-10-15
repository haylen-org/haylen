-- The automatic run of the headless platform: it walks through every screen of the app, places an order and follows it to the door, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Basket = require('basket')
local Catalog = require('data.catalog')
local Orders = require('orders')
local Shell = require('shell')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local restaurant = Catalog.restaurants[1]
    return {
        {'Home', function() end},
        {'Search for pizza', function() Shell.showTab('search', {cuisine = 'pizza'}) end},
        {'Open a restaurant', function() Shell.open(require('screens.restaurant'), {restaurant = restaurant.id}) end},
        {'Open a dish', function() Catalog.menu(restaurant) scene.top():showDish(restaurant.dishes[2]) end},
        {'Add the dish', function() scene.top():addChosen() scene.top().gui:set('dish', {open = false}) end},
        {'Add another dish', function() Basket.add(restaurant.dishes[5]) end},
        {'Jump to the drinks', function() scene.top():jumpTo('drinks') end},
        {'Open the cart', function() Shell.open(require('screens.cart')) end},
        {'Change a quantity', function() Basket.setQuantity(Basket.lines[1].id, 3) end},
        {'Open the checkout', function() Shell.open(require('screens.checkout')) end},
        {'Place the order', function() scene.top():place() end},
        {'Follow the kitchen', function() end},
        {'Follow the courier', function() Orders.active[1].elapsed = Orders.stageStart(Orders.active[1], 4) + 4 end},
        {'Go back to the orders', function() Shell.back() end},
        {'Open the account', function() Shell.showTab('account') end},
        {'Turn on the dark theme', function() ui.setTheme('foodDark') end},
        {'Track in the dark theme', function() Shell.open(require('screens.tracking'), {order = Orders.active[1].id}) end},
        {'Deliver the order', function() Orders.active[1].elapsed = Orders.stageStart(Orders.active[1], 5) end},
        {'Go home', function() Shell.showTab('home') ui.setTheme('foodLight') end},
    }
end

function Tour.start()
    local steps = Tour.steps()
    local index = 0
    timer.every(Tour.interval, function()
        index = index + 1
        if index > #steps then
            log.info('The tour finished.')
            haylen.quit()
            return
        end
        log.info(string.format('Step %d of %d: %s.', index, #steps, steps[index][1]))
        steps[index][2]()
    end, {owner = Tour})
end

return Tour
