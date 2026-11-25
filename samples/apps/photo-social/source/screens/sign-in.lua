-- The sheet that signs the person in: the email and the password, the button that sends them, a spinner while the sign-in service answers and the reason when it fails. It hands back the email of the account that signed in.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Account = require('account')
local Style = require('style')

local SignIn = haylen.class('SignIn', ui.Screen)

function SignIn:init()
    self.email, self.password = '', ''
end

function SignIn:build()
    local space = Style.space
    return ui.column{gap = space.m, padding = {space.m, space.l, space.l, space.l},
        ui.column{gap = space.xxs, align = 'stretch',
            ui.label{text = 'Sign in', font = 'heading', textAlign = 'center', align = 'stretch'},
            ui.label{text = 'Use any email to try the account of the app.', color = 'textMuted', textAlign = 'center', align = 'stretch'},
        },
        ui.formField{id = 'emailField', label = 'Email', ui.textField{id = 'email', placeholder = 'ana@example.com', keyboard = 'email', returnKey = 'next', maxLength = 120, onChange = function(event)
            self.email = event.value
        end}},
        ui.formField{id = 'passwordField', label = 'Password', help = string.format('A password shorter than %d characters is refused, to show a failed sign-in.', Account.minimumPassword),
            ui.secretField{id = 'password', returnKey = 'go', maxLength = 120, onChange = function(event)
                self.password = event.value
            end, onSubmit = function()
                self:submit()
            end},
        },
        ui.alert{id = 'failure', tone = 'danger', title = '', message = '', visible = false},
        ui.row{gap = space.s, align = 'stretch',
            ui.busyIndicator{id = 'busy', size = 32, visible = false},
            ui.button{id = 'submit', text = 'Sign in', variant = 'primary', grow = 1, onClick = function() self:submit() end},
        },
        ui.button{id = 'cancel', text = 'Cancel', variant = 'link', align = 'center', onClick = function() ui.goBack() end},
    }
end

-- Checks the fields on the device first, then sends them in a task of the sheet, so closing the sheet ends the request.
function SignIn:submit()
    if self.busy then
        return
    end
    local email = self.email:match('^%s*(.-)%s*$')
    local emailProblem = email:match('^[^@%s]+@[^@%s]+%.[^@%s]+$') and '' or 'Type an email such as ana@example.com.'
    local passwordProblem = self.password == '' and 'Type the password.' or ''
    self.gui:set('emailField', {error = emailProblem})
    self.gui:set('passwordField', {error = passwordProblem})
    self.gui:set('failure', {visible = false})
    if emailProblem ~= '' or passwordProblem ~= '' then
        return
    end

    self:showBusy(true)
    scene.spawn(self, function()
        local account, problem = Account.signIn(self, email, self.password)
        self:showBusy(false)
        if not account then
            self.gui:set('failure', {visible = true, title = problem.title, message = problem.message})
            return
        end
        self.presentation:dismiss({result = account.email})
    end)
end

function SignIn:showBusy(busy)
    self.busy = busy
    self.gui:set('email', {enabled = not busy})
    self.gui:set('password', {enabled = not busy})
    self.gui:set('submit', {text = busy and 'Signing in' or 'Sign in', enabled = not busy})
    self.gui:set('busy', {visible = busy})
end

return SignIn
