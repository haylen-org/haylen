-- Lua API of the Google sign-in of Tiny Island, which Credential Manager answers on Android and Google Identity Services on the web. The game loads it with require('google-sign-in').
local platform = require('haylen.platform')

local handle = platform.plugin('google-sign-in')
local googleSignIn = {}

-- Whether the native part of the plugin runs on this platform, which Android and the web have.
googleSignIn.available = handle.native

-- Asks the player for a Google account and answers with {idToken, email, name, picture}, or fails when the player closes the prompt or the game has no client id.
function googleSignIn.signIn()
    return handle:call('signIn')
end

return googleSignIn
