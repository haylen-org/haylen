-- The automatic run of the headless platform: it browses with filters, opens a product, picks a color and a size, fills the bag, applies a code, checks out with a missing field, follows the order on the map, saves a product and switches the theme, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Bag = require('bag')
local Orders = require('orders')
local Shell = require('shell')
local Store = require('data.store')
local Wishlist = require('wishlist')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local product
    for _, candidate in ipairs(Store.products) do
        if #Store.category(candidate.category).sizes > 1 and #candidate.colors > 2 then
            product = candidate
            break
        end
    end
    return {
        {'Shop', function() end},
        {'Browse a category', function() Shell.showTab('browse', {category = 'sneakers'}) end},
        {'Open the filters', function() scene.top():openFilters() end},
        {'Apply the filters', function() local page = scene.top() page.gui:set('filterDialog', {open = false}) page.pending.sale = true page.filters = page.pending page:refresh() end},
        {'Open a product', function() Shell.open(require('screens.product'), {product = product.id}) end},
        {'Pick a color', function() scene.top():pickColor(product.colors[3]) end},
        {'Add without a size', function() scene.top():addToBag() end},
        {'Add to the bag', function() local page = scene.top() page.size = Store.category(product.category).sizes[2] page.gui:set('size', {selected = page.size}) page:addToBag() end},
        {'Read the reviews', function() local page = scene.top() page.gui:set('addedDialog', {open = false}) page.gui:set('reviewDialog', {open = true}) end},
        {'Open the bag', function() scene.top().gui:set('reviewDialog', {open = false}) Bag.add(Store.products[7], Store.products[7].colors[1], Store.category(Store.products[7].category).sizes[1], 2) Shell.open(require('screens.bag')) end},
        {'Apply a promo code', function() local page = scene.top() page.code = 'welcome10' page.gui:set('code', {value = 'welcome10'}) page:applyCode() end},
        {'Open the checkout', function() Shell.open(require('screens.checkout')) end},
        {'Place without a street', function() local page = scene.top() page.fields.street = '' page.gui:set('street', {value = ''}) page:place() end},
        {'Place the order', function() local page = scene.top() page.fields.street = '12 Harbor Lane' page.gui:set('street', {value = '12 Harbor Lane'}) page:place() end},
        {'Follow the order', function() scene.top().gui:command('track', 'focus') Shell.replace(require('screens.order'), {order = Orders.list[1].id}) end},
        {'Ship the order', function() Orders.list[1].elapsed = Orders.stageStart(Orders.list[1], 4) + 3 end},
        {'Open the orders', function() Shell.back() end},
        {'Save a product', function() Wishlist.toggle(Store.products[11]) Wishlist.toggle(Store.products[12]) Shell.showTab('wishlist') end},
        {'Open the account', function() Shell.showTab('account') end},
        {'Turn on the dark theme', function() ui.setTheme('storeDark') Shell.showTab('home') end},
        {'Product in the dark theme', function() Shell.open(require('screens.product'), {product = product.id}) end},
        {'Back to the light theme', function() ui.setTheme('storeLight') Shell.showTab('home') end},
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
