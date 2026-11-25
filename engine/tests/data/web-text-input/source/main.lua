-- The app of the text input check of the web: a sheet with a text field and a secret field far apart, whose surface takes the presses between them, and a log line with what both fields hold whenever one changes, so `haylen test --target web` sees that a tap or a click on a field and the keys typed right after it reach that field.
local log = require('haylen.log')
local ui = require('haylen.ui')

local values = {name = '', secret = ''}

local function report(id, event)
    values[id] = event.value
    log.info(string.format('The fields hold "%s" and "%s".', values.name, values.secret))
end

local Form = {}

function Form:build()
    return ui.column{gap = 160, padding = 24,
        ui.textField{id = 'name', onChange = function(event) report('name', event) end},
        ui.secretField{id = 'secret', onChange = function(event) report('secret', event) end},
    }
end

ui.present(Form, {detents = {'fit'}})
log.info('The text input check is ready.')
