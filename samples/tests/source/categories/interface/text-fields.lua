-- One text field per on-screen keyboard and return key, a secret field, a text area and a filter field, with the change and submit events they report.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local TextFields = haylen.class('TextFields', Test)

-- Every keyboard of `textField`, each with one of the return key labels.
TextFields.fields = {
    {keyboard = 'text', returnKey = 'next', placeholder = 'Island name', autocapitalize = 'words'},
    {keyboard = 'number', returnKey = 'done', placeholder = '42'},
    {keyboard = 'decimal', returnKey = 'next', placeholder = '3.14'},
    {keyboard = 'phone', returnKey = 'go', placeholder = '+55 11 5555 0000'},
    {keyboard = 'email', returnKey = 'send', placeholder = 'captain@island.dev'},
    {keyboard = 'url', returnKey = 'go', placeholder = 'https://haylen.dev'},
    {keyboard = 'search', returnKey = 'search', placeholder = 'Search the map'},
}

function TextFields:enter()
    self:frame{
        hint = 'Phones, tablets, TVs and mobile browsers open the keyboard each field asks for, with the return key it names. Tab, or a return key labelled "next", moves to the next field, and Escape brings back the text a field had when it took the focus.',
        focus = 'field-text',
        content = {self:columns()},
    }
end

function TextFields:report(name)
    return function(event)
        self:set('status', {text = string.format('Field "%s", event "%s", value "%s".', name, event.name, event.value)})
    end
end

function TextFields:columns()
    local cells = {}
    for index, field in ipairs(TextFields.fields) do
        cells[index] = ui.formField{label = 'Keyboard "' .. field.keyboard .. '"', help = 'Return key "' .. field.returnKey .. '"',
            ui.textField{id = 'field-' .. field.keyboard, keyboard = field.keyboard, returnKey = field.returnKey, placeholder = field.placeholder, autocapitalize = field.autocapitalize, onChange = self:report(field.keyboard), onSubmit = self:report(field.keyboard)},
        }
    end
    cells[#cells + 1] = ui.formField{label = 'Default return key, 12 characters at most', help = 'No autocorrection',
        ui.textField{placeholder = 'Short code', maxLength = 12, autocorrect = false, autocapitalize = 'characters', onChange = self:report('code'), onSubmit = self:report('code')},
    }
    cells[#cells + 1] = ui.formField{label = 'Component "secretField"', help = 'The password keyboard',
        ui.secretField{placeholder = 'Password', returnKey = 'go', onSubmit = function(event)
            self:set('status', {text = string.format('The password was submitted with %d characters.', utf8.len(event.value))})
        end},
    }
    cells[#cells + 1] = ui.formField{label = 'Component "filterField"', help = 'The search keyboard and a clear button',
        ui.filterField{placeholder = 'Filter recipes', onChange = self:report('filter')},
    }
    return layout.columns{
        ui.grid{columns = 3, gap = 20, grow = 3, children = cells},
        layout.section('Component "textArea"', {grow = 1,
            ui.textArea{rows = 6, value = 'Dear diary,\nthe raft is ready.', onChange = function(event)
                self:set('status', {text = string.format('The diary has %d characters.', utf8.len(event.value))})
            end},
            ui.label{text = 'Enter starts a new line here, so a text area never submits.', color = 'textMuted'},
        }),
    }
end

return TextFields
