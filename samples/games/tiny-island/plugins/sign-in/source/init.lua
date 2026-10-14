-- Lua API of the sign-in of Tiny Island, which Credential Manager answers on Android and the identity service of the account on the web. The game loads it with require('sign-in').
local platform = require('haylen.platform')

local handle = platform.plugin('sign-in')
local signIn = {}

-- Whether the native part of the plugin runs on this platform, which Android and the web have.
signIn.available = handle.native

-- Asks the player for an account and answers with {idToken, email, name, picture}, or fails when the player closes the prompt or the game has no client id.
function signIn.signIn()
    return handle:call('signIn')
end

return signIn
