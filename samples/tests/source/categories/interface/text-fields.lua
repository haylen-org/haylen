-- One text field per on-screen keyboard and return key, a secret field, a text area and a filter field, with the change and submit events they report.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local TextFields = haylen.class('TextFields', Test)

TextFields.code = [[
ui.textField{keyboard = 'email', returnKey = 'send', placeholder = 'captain@island.dev', onSubmit = function(event) send(event.value) end}
ui.textField{placeholder = 'Short code', maxLength = 12, autocorrect = false, autocapitalize = 'characters'}
ui.secretField{placeholder = 'Password', returnKey = 'go'}
ui.filterField{placeholder = 'Filter recipes', onChange = function(event) filter(event.value) end}
ui.textArea{rows = 6, value = 'Dear diary,\nthe raft is ready and the wind turns west at dawn.'}  -- Long lines wrap.]]

-- Every keyboard of `textField`, each with one of the return key labels.
TextFields.fields = {
    {keyboard = 'text', returnKey = 'next', placeholder = 'Island name', autocapitalize = 'words'},
    {keyboard = 'number', returnKey = 'done', placeholder = 'Crew size'},
    {keyboard = 'decimal', returnKey = 'next', placeholder = 'Speed in knots'},
    {keyboard = 'phone', returnKey = 'go', placeholder = '+55 11 5555 0000'},
    {keyboard = 'email', returnKey = 'send', placeholder = 'captain@island.dev'},
    {keyboard = 'url', returnKey = 'go', placeholder = 'https://haylen.dev'},
    {keyboard = 'search', returnKey = 'search', placeholder = 'Search the map'},
}

TextFields.diary = 'Dear diary,\nthe raft is ready, the water barrels are full and the wind turns west at dawn, so we sail for the far island before the storm.'

function TextFields:enter()
    self:frame{
        hint = 'Phones, tablets, TVs and mobile browsers open the keyboard each field asks for, with the return key it names. Tab, or a return key labelled "next", moves to the next field, and Escape brings back the text a field had when it took the focus.',
        code = TextFields.code,
        focus = 'field-text',
        content = {self:columns()},
    }
end

function TextFields:report(name)
    return function(event)
        self:set('status', {text = string.format('The field "%s" reported "%s" with "%s".', name, event.name, event.value)})
    end
end

function TextFields:columns()
    local keyboards = {}
    for index, field in ipairs(TextFields.fields) do
        keyboards[index] = ui.formField{label = 'Keyboard "' .. field.keyboard .. '"', help = 'Return key "' .. field.returnKey .. '"',
            ui.textField{id = 'field-' .. field.keyboard, keyboard = field.keyboard, returnKey = field.returnKey, placeholder = field.placeholder, autocapitalize = field.autocapitalize, onChange = self:report(field.keyboard), onSubmit = self:report(field.keyboard)},
        }
    end
    local others = {
        ui.formField{label = 'Twelve characters at most', help = 'No autocorrection',
            ui.textField{placeholder = 'Short code', maxLength = 12, autocorrect = false, autocapitalize = 'characters', onChange = self:report('code'), onSubmit = self:report('code')},
        },
        ui.formField{label = 'Component "secretField"', help = 'The password keyboard',
            ui.secretField{placeholder = 'Password', returnKey = 'go', onSubmit = function(event)
                self:set('status', {text = string.format('The password was submitted with %d characters.', utf8.len(event.value))})
            end},
        },
        ui.formField{label = 'Component "filterField"', help = 'The search keyboard and a clear button',
            ui.filterField{placeholder = 'Filter recipes', onChange = self:report('filter')},
        },
    }
    return layout.columns{
        ui.column{grow = 3, gap = 24,
            layout.section('Keyboards and return keys', {ui.grid{columns = 3, gap = 20, rowGap = 28, children = keyboards}}),
            layout.section('Limits, secrets and filters', {ui.grid{columns = 3, gap = 20, rowGap = 28, children = others}}),
        },
        layout.section('Component "textArea"', {grow = 1,
            ui.textArea{rows = 6, value = TextFields.diary, onChange = function(event)
                self:set('status', {text = string.format('The diary has %d characters.', utf8.len(event.value))})
            end},
            ui.label{text = 'Long lines wrap at the width of the area, and Enter starts a new line, so a text area never submits.', color = 'textMuted'},
        }),
    }
end

return TextFields
