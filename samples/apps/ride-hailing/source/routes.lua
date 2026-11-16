-- The routes of the app: the location of every page that a link opens over the map, such as "trips", "trip/12" or "wallet". The links of the scheme of the app, "haylenride://trip/12", and of its web host, "https://ride.example.com/trip/12", open them from a cold start and while the app runs, and the pages that the menu opens go through the same routes, so the router knows where every page leads and builds the stack again after the system ended the app. A trip lies over the list of the trips, so the back button goes to the list and then to the map.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Page = require('page')
local Rides = require('rides')
local Shell = require('shell')

local Routes = {}

Routes.scheme = 'haylenride'
Routes.host = 'ride.example.com'

-- What a link to a trip that the rider never took shows, with the way back.
local Missing = haylen.class('Missing', Page)

function Missing:tree()
    return ui.column{gap = 0,
        Shell.header(''),
        ui.emptyState{grow = 1, image = Shell.icon('receipt'), imageSize = 72, title = 'Nothing here', message = self.params.text},
    }
end

-- Creates the router over the stack of the app, which lives as long as its owner.
function Routes.start(stack, owner)
    return ui.router{stack = stack, schemes = {Routes.scheme}, hosts = {Routes.host}, restore = true, owner = owner, routes = {
        {name = 'map', path = 'map'},
        {name = 'trips', path = 'trips', screen = Routes.tripsScreen},
        {name = 'trip', path = 'trip/{id:integer}', parent = 'trips', screen = Routes.tripScreen},
        {name = 'wallet', path = 'wallet', screen = Routes.walletScreen},
        {name = 'account', path = 'account', screen = Routes.accountScreen},
    }}
end

-- The location of a past trip, from the number of its id.
function Routes.trip(trip)
    return 'trip/' .. trip.id:match('(%d+)$')
end

function Routes.link(location)
    return Routes.scheme .. '://' .. location
end

function Routes.webLink(location)
    return 'https://' .. Routes.host .. '/' .. location
end

function Routes.tripsScreen()
    return require('screens.activity')()
end

function Routes.tripScreen(parameters)
    local id = 'trip-' .. parameters.id
    if not Rides.pastTrip(id) then
        return Missing({text = 'This trip is not among your trips.'})
    end
    return require('screens.trip-detail')({trip = id})
end

function Routes.walletScreen()
    return require('screens.wallet')()
end

function Routes.accountScreen()
    return require('screens.account')()
end

return Routes
