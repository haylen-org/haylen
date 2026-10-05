-- The fake sign-in of an SDK: the person picks an account in a native sheet, which is a screen of the plugin that covers the app, the account comes back with a token, the device keeps it as the current user, and signing out forgets it. The native part tells the app of every change with "userChanged".
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local SignIn = haylen.class('SignIn', DemoTest)

function SignIn:enter()
    self:frame{
        hint = 'Sign in with an account or cancel, check the current user, then sign out.',
        focus = 'signIn',
        controls = {
            ui.button{id = 'signIn', text = 'Sign in', variant = 'primary', onClick = function() self:signIn() end},
            ui.button{id = 'current', text = 'Check the current user', onClick = function() self:current() end},
            ui.button{id = 'signOut', text = 'Sign out', onClick = function() self:signOut() end},
            ui.label{text = 'The accounts are fake, and no password or network takes part.', color = 'textMuted', font = 'caption'},
        },
        code = "local account, err = demo.signIn():await()\ndemo.onUserChanged(function(user) print(user and user.name) end)\nlocal current = demo.currentUser():await()\ndemo.signOut():await()",
    }
    if self.native then
        self.connection = demo.onUserChanged(function(user)
            self.results:set('changed', 'pass', 'The event "userChanged" follows the account', user and string.format('Signed in as "%s".', user.name) or 'Signed out.')
        end)
    end
end

function SignIn:exit()
    if self.connection then
        self.connection:disconnect()
    end
    SignIn.super.exit(self)
end

function SignIn:signIn()
    self:act(function()
        local name = 'The sign-in answers with an account'
        self.results:set('signIn', 'waiting', name, 'The sign-in sheet shows. Pick an account or cancel.')
        local account, err = demo.signIn():await()
        if err and err.code == 'cancelled' then
            self.results:set('signIn', 'pass', name, 'The person cancelled, and the call failed with the code "cancelled".')
            return
        end
        if err then
            self.results:failure('signIn', name, err)
            return
        end
        self.results:set('signIn', 'pass', name, string.format('%s signed in "%s" <%s> with a token of %d characters.', account.language, account.name, account.email, #account.token))
    end)
end

function SignIn:current()
    self:act(function()
        local name = 'The device keeps the current user'
        local account, err = demo.currentUser():await()
        if err then
            self.results:failure('current', name, err)
            return
        end
        self.results:set('current', 'pass', name, account and string.format('The current user is "%s".', account.name) or 'Nobody is signed in.')
    end)
end

function SignIn:signOut()
    self:act(function()
        local name = 'Signing out forgets the account'
        local _, err = demo.signOut():await()
        if err then
            self.results:failure('signOut', name, err)
            return
        end
        local account = demo.currentUser():await()
        self.results:set('signOut', account == nil and 'pass' or 'fail', name, account == nil and 'Nobody is signed in anymore.' or 'The account stayed after signing out.')
    end)
end

return SignIn
