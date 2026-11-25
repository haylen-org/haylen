-- The account of the person, kept in memory, so every start begins signed out. Signing in posts the email and the password as JSON to an echo service that stands in for a sign-in API: a password shorter than the demo rule goes to an address that answers 401, and any other answers with the JSON it received, whose email becomes the account.
local events = require('haylen.events')
local http = require('http')
local timer = require('haylen.timer')

local Account = {}

Account.service = 'https://httpbin.org'
Account.timeout = 10
Account.minimumPassword = 8
Account.email = nil

-- Signs in inside a task of `owner`, which also owns the deadline, so a sheet that closes takes its request with it. Returns the account, or nil and the problem with a title and a message for the person.
function Account.signIn(owner, email, password)
    local address = Account.service .. (#password < Account.minimumPassword and '/status/401' or '/post')
    local controller <close> = http.client.controller()
    local expired = false
    local deadline = timer.after(Account.timeout, function()
        expired = true
        controller:cancel()
    end, {owner = owner})
    local response, failure = http.client.post(address, {json = {email = email, password = password}, controller = controller}):await()
    timer.cancel(deadline)

    if expired then
        return nil, {title = 'The sign-in took too long', message = string.format('The sign-in service did not answer within %d seconds. Try again.', Account.timeout)}
    end
    if not response then
        return nil, {title = 'No connection', message = string.format('%s Check the connection and try again.', tostring(failure))}
    end
    if response.status == 401 then
        return nil, {title = 'Wrong email or password', message = string.format('The sign-in service refused the password. The passwords of this demo have at least %d characters.', Account.minimumPassword)}
    end
    if response.status < 200 or response.status >= 300 then
        return nil, {title = 'The sign-in failed', message = string.format('The sign-in service answered with status %d. Try again later.', response.status)}
    end

    local parsed, answer = pcall(response.json)
    local echoed = parsed and type(answer) == 'table' and type(answer.json) == 'table' and answer.json.email
    if type(echoed) ~= 'string' or echoed == '' then
        return nil, {title = 'The sign-in failed', message = 'The sign-in service answered without the account. Try again later.'}
    end
    Account.email = echoed
    events.emit('accountChanged', {email = echoed})
    return {email = echoed}
end

function Account.signOut()
    Account.email = nil
    events.emit('accountChanged', {})
end

return Account
