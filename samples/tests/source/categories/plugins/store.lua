-- The fake store of an SDK: the products of the catalog as cards that open as the test does, a purchase that the person confirms in a native sheet, which is a screen of the plugin that covers the app, a receipt and a transaction, the product that the device keeps, and the purchases restored. The native part tells the app of every purchase with "purchaseUpdated", the way a store SDK reports transactions. Nothing is charged.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Store = haylen.class('Store', DemoTest)

-- The picture of each kind of product: coins the person buys again and again, and a product that the device keeps.
Store.icons = {consumable = 'plugins/coins.svg', nonConsumable = 'plugins/no-ads.svg'}

function Store:enter()
    self.updates = 0
    self.owned = {}
    self:frame{
        hint = 'Buy a product or cancel its sheet, then restore the purchases.',
        focus = 'restore',
        checksEmpty = 'The checks show once the store answers.',
        controls = {
            ui.button{id = 'restore', text = 'Restore the purchases', variant = 'primary', onClick = function() self:restore() end},
            ui.button{id = 'products', text = 'Load the products again', onClick = function() self:loadProducts() end},
            ui.label{text = 'The purchase sheet is an alert of UIKit or AppKit on Apple platforms, a dialog on Android, a dialog element on the web and a window over the app on the desktops. It is a screen of the plugin, so the app is covered while it shows.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 16,
                ui.row{gap = 12, ui.label{text = 'The fake store', font = 'heading', grow = 1}, ui.badge{text = 'Nothing is charged', tone = 'information'}},
                ui.scroll{grow = 1, height = 0, ui.column{id = 'shelf', gap = 12, align = 'stretch',
                    ui.row{gap = 16, align = 'center', ui.busyIndicator{size = 40}, ui.label{text = 'Loading the products of the store.', color = 'textMuted'}},
                }},
            },
        },
        code = "local products = demo.products():await()\nlocal purchase, err = demo.purchase('coins.small'):await()\ndemo.onPurchaseUpdated(function(update) print(update.state) end)\nlocal kept = demo.restorePurchases():await()",
    }
    if not self.native then
        self.gui:replaceChildren('shelf', {ui.label{text = 'The store needs the native part of the plugin.', color = 'textMuted'}})
        return
    end
    self.connection = demo.onPurchaseUpdated(function(update)
        self.updates = self.updates + 1
        local owned = self:ownedOf(update.productId)
        owned.bought = owned.bought + (update.state == 'purchased' and 1 or 0)
        owned.kept = owned.kept or update.state == 'restored'
        self:showOwned(update.productId)
        self.results:set('updated', 'pass', 'The event "purchaseUpdated" reports each transaction', string.format('%d updates arrived, the last "%s" for "%s".', self.updates, update.state, update.productId))
    end)
    self:loadProducts()
end

function Store:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Store.super.exit(self)
end

function Store:ownedOf(productId)
    self.owned[productId] = self.owned[productId] or {bought = 0, kept = false}
    return self.owned[productId]
end

function Store:loadProducts()
    self:act(function()
        local name = 'The store lists its products'
        local products, err = demo.products():await()
        if err then
            self.results:failure('products', name, err)
            return
        end
        self.products = products
        local cards = {}
        for index, product in ipairs(products) do
            cards[index] = self:productCard(product)
        end
        self.gui:replaceChildren('shelf', cards)
        for _, product in ipairs(products) do
            self:showOwned(product.id)
        end
        self.results:set('products', #products > 0 and 'pass' or 'fail', name, string.format('The catalog of %s holds %d products with their prices.', #products > 0 and 'the native part' or 'nobody', #products))
    end)
end

-- A product with its picture, its price and what the device owns of it, and the button that opens its purchase sheet.
function Store:productCard(product)
    return ui.card{align = 'stretch', padding = {16, 20},
        ui.row{gap = 20,
            ui.icon{image = Store.icons[product.kind] or Store.icons.consumable, size = 88},
            ui.column{grow = 1, gap = 4,
                ui.label{text = product.title, font = 'heading'},
                ui.label{text = product.description, color = 'textMuted'},
                ui.badge{id = 'owned-' .. product.id, text = '', tone = 'success', visible = false},
            },
            ui.button{id = 'buy-' .. product.id, text = string.format('%s %s', product.price, product.currency), variant = 'primary', onClick = function() self:buy(product) end},
        },
    }
end

function Store:showOwned(productId)
    if not self.gui:has('owned-' .. productId) then
        return
    end
    local owned = self:ownedOf(productId)
    local text = owned.kept and 'Kept on this device' or owned.bought > 0 and string.format('Bought %d %s', owned.bought, owned.bought == 1 and 'time' or 'times') or ''
    self:set('owned-' .. productId, {text = text, visible = text ~= ''})
end

function Store:buy(product)
    self:act(function()
        local name = 'The purchase of "' .. product.id .. '" answers'
        self.results:set(product.id, 'waiting', name, 'The purchase sheet shows. Buy or cancel.')
        local purchase, err = demo.purchase(product.id):await()
        if err and err.code == 'cancelled' then
            self.results:set(product.id, 'pass', name, 'The person cancelled, and the call failed with the code "cancelled".')
            return
        end
        if err then
            self.results:failure(product.id, name, err)
            return
        end
        self.results:set(product.id, 'pass', name, string.format('%s bought it in the transaction "%s" with a receipt of %d characters.', purchase.language, purchase.transactionId, #purchase.receipt))
    end)
end

function Store:restore()
    self:act(function()
        local name = 'The kept purchases come back'
        local kept, err = demo.restorePurchases():await()
        if err then
            self.results:failure('restore', name, err)
            return
        end
        local products = {}
        for index, purchase in ipairs(kept) do
            products[index] = '"' .. purchase.productId .. '"'
        end
        self.results:set('restore', 'pass', name, #kept > 0 and 'The device keeps ' .. table.concat(products, ', ') .. '.' or 'The device keeps no purchase yet. Buy no more ads to keep one.')
    end)
end

return Store
