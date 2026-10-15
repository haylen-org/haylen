-- The automatic run of the headless platform: it walks through every screen of the app, filters the listings, browses the map, makes an offer, chats, publishes a listing and switches the theme, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Chats = require('chats')
local Market = require('data.market')
local Saved = require('saved')
local Shell = require('shell')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local listing = Market.listings[3]
    return {
        {'Browse', function() end},
        {'Pick a category', function() scene.top():pickCategory('bikes') end},
        {'Open the filters', function() scene.top():openFilters() end},
        {'Apply a price range', function() local page = scene.top() page.gui:set('filterDialog', {open = false}) page.low, page.high = 50, 300 page:refresh() end},
        {'Show the map', function() local page = scene.top() page.gui:set('view', {selected = 'map'}) page:showView('map') end},
        {'Pan the map', function() local page = scene.top() page:centerOn(page.shown[5]) page:zoomBy(1.4) end},
        {'Open a listing', function() Saved.toggle(listing) Shell.open(require('screens.listing'), {listing = listing.id}) end},
        {'Turn the gallery', function() scene.top().gui:set('gallery', {page = 2}) end},
        {'Make an offer', function() scene.top():openOffer() end},
        {'Send the offer', function() local page = scene.top() page.gui:set('offerDialog', {open = false}) page:sendOffer() end},
        {'Read the answer', function() end},
        {'Open the seller', function() Shell.open(require('screens.seller'), {seller = listing.seller}) end},
        {'Read the reviews', function() local page = scene.top() page.gui:set('section', {selected = 'reviews'}) page:show('reviews') end},
        {'Open the inbox', function() Shell.showTab('inbox') end},
        {'Open a chat', function() Shell.open(require('screens.chat'), {conversation = Chats.list[2].id}) end},
        {'Write a message', function() scene.top():send('Is the price firm?') end},
        {'Open the sell form', function() Shell.showTab('sell') end},
        {'Publish without a photo', function() scene.top():publish() end},
        {'Fill the form', function()
            local page = scene.top()
            page:pickPhoto('guitar', 'music')
            page:pickBackdrop('lemon')
            page.fields.title = 'Travel guitar with bag'
            page.gui:set('title', {value = page.fields.title})
            page.gui:set('photoDialog', {open = true})
        end},
        {'Publish the listing', function() local page = scene.top() page.gui:set('photoDialog', {open = false}) page:publish() end},
        {'Open the saved tab', function() Shell.showTab('saved') end},
        {'Turn on the dark theme', function() ui.setTheme('marketDark') Shell.showTab('profile') end},
        {'Map in the dark theme', function() Shell.showTab('home') end},
        {'Show the dark map', function() local page = scene.top() page.gui:set('view', {selected = 'map'}) page:showView('map') end},
        {'Go back to the light theme', function() ui.setTheme('marketLight') Shell.showTab('home') end},
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
