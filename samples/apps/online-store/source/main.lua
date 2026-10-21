-- Online Store: a made-up shop of clothes, shoes, bags and things for the home, with three thousand products in categories, product pages with a gallery, colors and sizes, a bag with promo codes, the checkout, orders that move through their stages and saved products, in a bright and airy look of warm paper, ink and a serif.
local events = require('haylen.events')
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Style = require('style')

Style.register()

local Shell = require('shell')
local Tour = require('tour')
local Widgets = require('widgets')

-- The interface keeps its size in points on every screen, and a television, seen from the couch, draws it half as large again.
if system.info().deviceKind == 'tv' then
    ui.setScale(1.5)
end
ui.setTheme('store')
Widgets.registerSwatches()

events.on('notice', function(text)
    Shell.notify(text, 'information')
end)

Shell.start()

if haylen.platform == 'headless' then
    Tour.start()
end
