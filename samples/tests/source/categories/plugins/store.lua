-- The fake store of an SDK: the products of the catalog, a purchase that the person confirms in a native sheet, which is a screen of the plugin that covers the app, a receipt and a transaction, the product that the device keeps, and the purchases restored. The native part tells the app of every purchase with "purchaseUpdated", the way a store SDK reports transactions. Nothing is charged.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Store = haylen.class('Store', DemoTest)

function Store:enter()
    self.updates = 0
    self:frame{
        hint = 'Load the products, buy one or cancel, then restore the purchases.',
        focus = 'products',
        controls = {
            ui.button{id = 'products', text = 'Load the products', variant = 'primary', onClick = function() self:loadProducts() end},
            ui.button{id = 'coins', text = 'Buy a pouch of coins', onClick = function() self:buy('coins.small') end},
            ui.button{id = 'ads', text = 'Buy no more ads', onClick = function() self:buy('ads.remove') end},
            ui.button{id = 'restore', text = 'Restore the purchases', onClick = function() self:restore() end},
            ui.label{text = 'An alert of UIKit or AppKit on Apple platforms, a dialog on Android, a dialog element on the web and a window over the app on the desktops. The purchase is a screen of the plugin, so the app is covered while it shows.', color = 'textMuted', font = 'caption'},
        },
        code = "local products = demo.products():await()\nlocal purchase, err = demo.purchase('coins.small'):await()\ndemo.onPurchaseUpdated(function(update) print(update.state) end)\nlocal kept = demo.restorePurchases():await()",
    }
    if self.native then
        self.connection = demo.onPurchaseUpdated(function(update)
            self.updates = self.updates + 1
            self.results:set('updated', 'pass', 'The event "purchaseUpdated" reports each transaction', string.format('%d updates arrived, the last "%s" for "%s".', self.updates, update.state, update.productId))
        end)
    end
end

function Store:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Store.super.exit(self)
end

function Store:loadProducts()
    self:act(function()
        local name = 'The store lists its products'
        local products, err = demo.products():await()
        if err then
            self.results:failure('products', name, err)
            return
        end
        local titles = {}
        for index, product in ipairs(products) do
            titles[index] = string.format('"%s" for %s %s', product.title, product.price, product.currency)
        end
        self.results:set('products', #products > 0 and 'pass' or 'fail', name, table.concat(titles, ', ') .. '.')
    end)
end

function Store:buy(productId)
    self:act(function()
        local name = 'The purchase of "' .. productId .. '" answers'
        self.results:set(productId, 'waiting', name, 'The purchase sheet shows. Buy or cancel.')
        local purchase, err = demo.purchase(productId):await()
        if err and err.code == 'cancelled' then
            self.results:set(productId, 'pass', name, 'The person cancelled, and the call failed with the code "cancelled".')
            return
        end
        if err then
            self.results:failure(productId, name, err)
            return
        end
        self.results:set(productId, 'pass', name, string.format('%s bought it in the transaction "%s" with a receipt of %d characters.', purchase.language, purchase.transactionId, #purchase.receipt))
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
