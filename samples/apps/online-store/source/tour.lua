-- The automatic run of the headless platform: it browses a category with filters, opens a product, shares it, picks a color and a size, fills the bag, applies a code, removes a product, asks before emptying the bag, checks out with a missing field, follows the order through its stages, saves products, opens the account and opens links of the scheme and of the web, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Bag = require('bag')
local Orders = require('orders')
local Routes = require('routes')
local Shell = require('shell')
local Store = require('data.store')
local Wishlist = require('wishlist')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local product
    for _, candidate in ipairs(Store.products) do
        if #Store.category(candidate.category).sizes > 1 and #candidate.colors > 2 and candidate.oldPrice then
            product = candidate
            break
        end
    end
    local other = Store.products[7]
    return {
        {'Shop', function() end},
        {'Browse a category', function() Shell.open(Routes.category('sneakers')) end},
        {'Open the filters', function() Shell.top():openFilters() end},
        {'Apply the filters', function() local sheet = Shell.top().filtersSheet sheet.screen.pending.sale = true sheet:dismiss({result = sheet.screen.pending}) end},
        {'Open a product', function() Shell.open(Routes.product(product)) end},
        {'Share the product', function() Shell.top():share() end},
        {'Copy its link', function() Shell.top().shareSheet:dismiss({result = 'web'}) end},
        {'Pick a color', function() Shell.top():pickColor(product.colors[3]) end},
        {'Add without a size', function() Shell.top():addToBag() end},
        {'Add to the bag', function() local page = Shell.top() page:pickSize(Store.category(product.category).sizes[2]) page:addToBag() end},
        {'Read the reviews', function() local page = Shell.top() page.addedSheet:dismiss() page:openReviews() end},
        {'Open the bag', function() Shell.top().reviewsSheet:dismiss() Bag.add(other, other.colors[1], Store.category(other.category).sizes[1], 2) Shell.openBag() end},
        {'Apply a promo code', function() local page = Shell.top() page.code = 'welcome10' page.gui:set('code', {value = 'welcome10'}) page:applyCode() end},
        {'Remove a product, which a snackbar brings back', function() Shell.top():removeLine(Bag.lines[2].id) end},
        {'Ask before emptying the bag', function() Shell.top():askEmpty() end},
        {'Keep the bag', function() Shell.top().emptyAlert:dismiss({result = 'keep'}) end},
        {'Open the checkout', function() local page = Shell.top() page.stack:push(require('screens.checkout')()) end},
        {'Place without a street', function() local page = Shell.top() page.fields.street = '' page.gui:set('street', {value = ''}) page:place() end},
        {'Place the order', function() local page = Shell.top() page.fields.street = '12 Harbor Lane' page.gui:set('street', {value = '12 Harbor Lane'}) page:place() end},
        {'Follow the order', function() local page = Shell.top() page.gui:command('track', 'focus') page:follow() end},
        {'Ship the order', function() Orders.advance(Orders.list[1], 4) end},
        {'Back to the orders', function() ui.goBack() end},
        {'Save products', function() Wishlist.toggle(Store.products[11]) Wishlist.toggle(Store.products[12]) Wishlist.toggle(Store.products[30]) Shell.showTab('saved') end},
        {'Open the account', function() Shell.showTab('account') end},
        {'Back to the shop, which kept its product', function() Shell.showTab('home') end},
        {'Open a link to a product', function() Shell.router:open(Routes.link(Routes.product(Store.products[42]))) end},
        {'Open a web link to a category', function() Shell.router:open(Routes.webLink(Routes.category('lighting'))) end},
        {'Open a link to a product the store lacks', function() Shell.router:open(Routes.link('product/999999')) end},
        {'Open the bag from a link', function() Shell.router:open(Routes.link('bag')) end},
        {'Close the bag', function() ui.goBack() end},
        {'Browse every product', function() Shell.showTab('browse') end},
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
