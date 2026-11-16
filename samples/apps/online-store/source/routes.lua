-- The routes of the app: the location of every screen that a link opens, such as "product/12", "category/sneakers" or "bag". The links of the scheme of the app, "haylenstore://product/12", and of its web host, "https://store.example.com/product/12", open them from a cold start and while the app runs, and the screens that the person opens by hand go through the same routes, so the router knows where every screen leads and builds the navigation again after the system ended the app.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')

local Routes = {}

Routes.scheme = 'haylenstore'
Routes.host = 'store.example.com'

-- What a link to something the store does not sell shows, with the way back.
local Missing = haylen.class('Missing', Page)

function Missing:tree()
    return ui.column{gap = 0,
        Shell.header(''),
        ui.emptyState{grow = 1, image = 'images/hero.png', imageSize = 220, title = 'Nothing here', message = self.params.text},
    }
end

-- Creates the router over the tabs of the app, which lives as long as its owner.
function Routes.start(tabs, owner)
    return ui.router{tabs = tabs, schemes = {Routes.scheme}, hosts = {Routes.host}, restore = true, owner = owner, routes = {
        {name = 'shop', path = 'shop', tab = 'home'},
        {name = 'browse', path = 'browse', tab = 'browse'},
        {name = 'category', path = 'category/{id}', tab = 'browse', screen = Routes.categoryScreen},
        {name = 'product', path = 'product/{id:integer}', screen = Routes.productScreen},
        {name = 'saved', path = 'saved', tab = 'saved'},
        {name = 'orders', path = 'orders', tab = 'orders'},
        {name = 'account', path = 'account', tab = 'account'},
        {name = 'bag', path = 'bag', present = {style = 'sheet', maxWidth = 1100, maxHeight = 1200}, screen = Routes.bagScreen},
    }}
end

-- The location of a product, from the number of its id.
function Routes.product(product)
    return 'product/' .. product.id:match('(%d+)$')
end

function Routes.category(id)
    return 'category/' .. id
end

function Routes.link(location)
    return Routes.scheme .. '://' .. location
end

function Routes.webLink(location)
    return 'https://' .. Routes.host .. '/' .. location
end

function Routes.categoryScreen(parameters)
    if not Store.category(parameters.id) then
        return Missing({text = 'The store has no such category.'})
    end
    return require('screens.browse')({category = parameters.id})
end

function Routes.productScreen(parameters)
    local id = 'p' .. parameters.id
    if not Store.product(id) then
        return Missing({text = 'The store does not sell this product anymore.'})
    end
    return require('screens.product')({product = id})
end

-- The flow of the bag is a presentation, which the shell keeps to know when it shows.
function Routes.bagScreen()
    Shell.bagFlow = require('screens.checkout-flow')()
    return Shell.bagFlow
end

return Routes
