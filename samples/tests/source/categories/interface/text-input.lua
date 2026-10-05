-- Text fields edit through a hidden native field of the platform, which brings the on-screen keyboard, input methods, dictation and native paste, and the UI moves above the keyboard. The plain keyboard types without a text field.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local TextInput = haylen.class('TextInput', Test)

TextInput.platforms = [==[
[b]Where the hidden field lives[/b]
[ul]
[b]Web:[/b] a hidden [code]textarea[/code], or an [code]input[/code] for passwords, over the field, focused inside the tap on iOS.
[b]Android:[/b] a hidden [code]EditText[/code] with the input connection, suggestions and the keyboard insets.
[b]iOS and iPadOS:[/b] a hidden [code]UITextField[/code] or [code]UITextView[/code] with marked text, dictation and emoji.
[b]tvOS:[/b] the same field opens the fullscreen keyboard of the TV.
[b]macOS:[/b] a hidden [code]NSTextView[/code] with dead keys, the emoji picker and dictation.
[b]Windows:[/b] the input method window placed at the caret. [b]Linux:[/b] key and character events.
[/ul]]==]

TextInput.lineCount = 7

function TextInput:init(entry)
    TextInput.super.init(self, entry)
    self.lines = {}
    self.typed = ''
    self.plain = false
end

function TextInput:enter()
    self:frame{
        hint = 'Focus the message field: on a phone or a tablet the keyboard opens and every GUI moves up above it. Try an input method such as Japanese, whose composing text shows underlined. The plain keyboard button types into the line above it without a text field.',
        focus = 'message',
        content = {self:columns()},
    }
    self:listen('keyboardShown', function(frame)
        self:record(string.format('Keyboard shown, top at %.0f', frame.y))
    end)
    self:listen('keyboardHidden', function()
        self:record('Keyboard hidden')
    end)
end

function TextInput:exit()
    if self.plain then
        window.setKeyboardVisible(false)
    end
    TextInput.super.exit(self)
end

function TextInput:record(line)
    table.insert(self.lines, 1, string.format('%6.2f  %s', haylen.elapsed(), line))
    self.lines[TextInput.lineCount + 1] = nil
    self:set('events', {text = table.concat(self.lines, '\n')})
end

function TextInput:columns()
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            ui.card{ui.richText{text = TextInput.platforms}},
            layout.section('Plain keyboard', {
                ui.label{id = 'typed', text = 'Nothing typed yet', font = 'monospace'},
                ui.button{id = 'plain', text = 'Show the plain keyboard', onClick = function(event)
                    self.plain = not self.plain
                    window.setKeyboardVisible(self.plain)
                    event.gui:set('plain', {text = self.plain and 'Hide the plain keyboard' or 'Show the plain keyboard'})
                end},
            }),
        },
        ui.column{grow = 1, gap = 24, align = 'stretch',
            layout.section('Events', {ui.label{id = 'events', text = 'Keyboard and text events show here.', font = 'monospace', color = 'textMuted'}}),
            ui.spacer{grow = 1},
            ui.formField{label = 'Message', help = 'The send key submits, and the field stays above the keyboard.',
                ui.textField{id = 'message', placeholder = 'Say hello to the keeper', returnKey = 'send', onSubmit = function(event)
                    self:record('Submitted "' .. event.value .. '"')
                    event.gui:set('message', {value = ''})
                end},
            },
            ui.textArea{rows = 2, placeholder = 'A text area at the bottom of the screen'},
        },
    }
end

-- The platform events behind the fields, and the typing of the plain keyboard, which arrives as characters and editing keys.
function TextInput:event(event)
    if event.type == 'textAction' then
        self:record('Text action "' .. event.action .. '"')
    elseif event.type == 'keyboardChanged' then
        self:record(string.format('Keyboard frame %.0f x %.0f', event.frame.width, event.frame.height))
    elseif self.plain and not ui.usingKeyboard() then
        if event.type == 'character' then
            self.typed = self.typed .. event.character
        elseif event.type == 'keyDown' and event.key == 'backspace' and #self.typed > 0 then
            self.typed = self.typed:sub(1, utf8.offset(self.typed, -1) - 1)
        end
        self:set('typed', {text = self.typed == '' and 'Nothing typed yet' or self.typed})
    end
end

function TextInput:update(dt)
    TextInput.super.update(self, dt)
    self:status(string.format('Platform "%s", the UI %s the keyboard, last device "%s".', haylen.platform, ui.usingKeyboard() and 'uses' or 'does not use', input.lastDevice()))
end

return TextInput
