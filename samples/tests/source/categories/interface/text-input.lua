-- Text fields edit through a hidden native field of the platform, which brings the on-screen keyboard, input methods, dictation and native paste, and the UI moves above the keyboard. The plain keyboard types without a text field.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local TextInput = haylen.class('TextInput', Test)

TextInput.code = [[
ui.textField{id = 'message', placeholder = 'Say hello to the keeper', returnKey = 'send', onSubmit = function(event)
    event.gui:set('message', {value = ''})
end}  -- It edits through a hidden native field, and the GUIs move above the keyboard.
events.on('keyboardShown', function(frame) log.info('The keyboard starts at ' .. frame.y .. '.') end, {owner = self})
window.setKeyboardVisible(true)  -- The plain keyboard sends "character" and "keyDown" events.]]

TextInput.platforms = '[table=2]'
    .. '[cell padding=8][b]Web[/b][/cell][cell padding=8]A hidden [code]textarea[/code], or an [code]input[/code] for passwords[/cell]'
    .. '[cell padding=8][b]Android[/b][/cell][cell padding=8]A hidden [code]EditText[/code] with suggestions and the keyboard insets[/cell]'
    .. '[cell padding=8][b]iPhone, iPad[/b][/cell][cell padding=8]A hidden [code]UITextField[/code] or [code]UITextView[/code] with dictation[/cell]'
    .. '[cell padding=8][b]Apple TV[/b][/cell][cell padding=8]The same field opens the fullscreen keyboard[/cell]'
    .. '[cell padding=8][b]Mac[/b][/cell][cell padding=8]A hidden [code]NSTextView[/code] with dead keys and emoji[/cell]'
    .. '[cell padding=8][b]Windows, Linux[/b][/cell][cell padding=8]Key and character events, with the input method at the caret[/cell]'
    .. '[/table]'
TextInput.platformNames = {web = 'the web', android = 'Android', ios = 'iOS', tvos = 'tvOS', macos = 'macOS', windows = 'Windows', linux = 'Linux', headless = 'the headless host'}
TextInput.devices = {keyboardMouse = 'the keyboard and mouse', gamepad = 'a gamepad', touch = 'a touch screen'}

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
        code = TextInput.code,
        focus = 'plain',
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
            layout.section('Fields that edit through the platform', {
                ui.formField{label = 'Message', help = 'The send key submits, and the field stays above the keyboard.',
                    ui.textField{id = 'message', placeholder = 'Say hello to the keeper', returnKey = 'send', onSubmit = function(event)
                        self:record('Submitted "' .. event.value .. '"')
                        event.gui:set('message', {value = ''})
                    end},
                },
                ui.textArea{rows = 2, placeholder = 'A text area, which wraps its long lines'},
            }),
            layout.section('Plain keyboard', {
                ui.label{id = 'typed', text = 'Nothing typed yet', font = 'monospace'},
                ui.button{id = 'plain', text = 'Show the plain keyboard', onClick = function(event)
                    self.plain = not self.plain
                    window.setKeyboardVisible(self.plain)
                    event.gui:set('plain', {text = self.plain and 'Hide the plain keyboard' or 'Show the plain keyboard'})
                end},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Events', {ui.label{id = 'events', text = 'Keyboard and text events show here.', font = 'monospace', color = 'textMuted'}}),
            layout.section('Where the hidden field lives', {ui.richText{text = TextInput.platforms}}),
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
    local platform = TextInput.platformNames[haylen.platform] or haylen.platform
    self:status(string.format('On %s the UI %s, and the last device is %s.', platform, ui.usingKeyboard() and 'edits a text field' or 'edits no text field', TextInput.devices[input.lastDevice()] or 'unknown'))
end

return TextInput
