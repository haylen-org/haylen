-- Text input: text fields edit through a hidden native field of the platform, which brings the on-screen keyboard, input methods, dictation and native paste, and the UI moves above the keyboard. The plain keyboard types without a text field.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')

local TextInput = haylen.class('TextInput', sample.Test)

TextInput.hints = 'Focus the message field at the bottom: on a phone or a tablet the keyboard opens and every document moves up above it. Try an input method such as Japanese, whose composing text shows underlined. The plain keyboard button types into the line above it without a text field.'
TextInput.focus = 'message'

local kPlatforms = [==[
[b]Where the hidden field lives[/b]
[ul]
[b]Web:[/b] a hidden [code]textarea[/code], or an [code]input[/code] for passwords, over the field, focused inside the tap on iOS.
[b]Android:[/b] a hidden [code]EditText[/code] with the input connection, suggestions and the keyboard insets.
[b]iOS and iPadOS:[/b] a hidden [code]UITextField[/code] or [code]UITextView[/code] with marked text, dictation and emoji.
[b]tvOS:[/b] the same field opens the fullscreen keyboard of the Apple TV.
[b]macOS:[/b] a hidden [code]NSTextView[/code] with dead keys, the emoji picker and dictation.
[b]Windows:[/b] the input method window placed at the caret. [b]Linux:[/b] key and character events.
[/ul]]==]

local kLogSize = 7

function TextInput:init(entry)
    TextInput.super.init(self, entry)
    self.log = {}
    self.typed = ''
    self.plain = false
end

function TextInput:record(line)
    table.insert(self.log, 1, string.format('%6.2f  %s', haylen.elapsed(), line))
    self.log[kLogSize + 1] = nil
    self.document:set('log', {text = table.concat(self.log, '\n')})
end

function TextInput:content()
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            ui.card{ui.richText{text = kPlatforms}},
            sample.section('plain keyboard', {
                ui.label{id = 'typed', text = 'Nothing typed yet', font = 'monospace'},
                ui.button{id = 'plain', text = 'Show the plain keyboard', onClick = function(event)
                    self.plain = not self.plain
                    window.setKeyboardVisible(self.plain)
                    event.document:set('plain', {text = self.plain and 'Hide the plain keyboard' or 'Show the plain keyboard'})
                end},
            }),
        },
        ui.column{grow = 1, gap = 24, align = 'stretch',
            sample.section('events', {ui.label{id = 'log', text = 'Keyboard and text events show here.', font = 'monospace', color = 'textMuted'}}),
            ui.spacer{grow = 1},
            ui.formField{label = 'Message', help = 'The send key submits, and the field stays above the keyboard.',
                ui.textField{id = 'message', placeholder = 'Say hello to the keeper', returnKey = 'send', onSubmit = function(event)
                    self:record('Submitted "' .. event.value .. '"')
                    event.document:set('message', {value = ''})
                end},
            },
            ui.textArea{rows = 2, placeholder = 'A text area at the bottom of the screen'},
        },
    }
end

function TextInput:started()
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
end

-- The platform events behind the fields, and the typing of the plain keyboard, which arrives as characters and editing keys.
function TextInput:event(event)
    if event.type == 'textAction' then
        self:record('text action ' .. event.action)
    elseif event.type == 'keyboardChanged' then
        self:record(string.format('keyboard frame %.0f x %.0f', event.frame.width, event.frame.height))
    elseif self.plain and not ui.usingKeyboard() then
        if event.type == 'character' then
            self.typed = self.typed .. event.character
        elseif event.type == 'keyDown' and event.key == 'backspace' and #self.typed > 0 then
            self.typed = self.typed:sub(1, utf8.offset(self.typed, -1) - 1)
        end
        self.document:set('typed', {text = self.typed == '' and 'Nothing typed yet' or self.typed})
    end
end

function TextInput:update(dt)
    self:setStatus(string.format('platform %s, the UI %s the keyboard, last device %s', haylen.platform, ui.usingKeyboard() and 'uses' or 'does not use', input.lastDevice()))
end

return TextInput
