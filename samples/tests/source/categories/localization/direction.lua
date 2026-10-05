-- Arabic declares that it reads right to left, and `ui.setDirection('auto')` lets the UI follow the language, so picking Arabic mirrors every row, check box, toggle, slider, list and text field while the text itself reads from the right. A node with a direction of its own keeps it whatever the language.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')

local Direction = haylen.class('Direction', LanguageTest)

Direction.hint = 'Pick Arabic to mirror the settings card and the frame of the test, and type into the fields of the other panel, which keep their own direction in every language.'

function Direction:content()
    return {
        ui.card{width = 760, align = 'start', gap = 18,
            ui.sectionTitle{text = {key = 'direction.settings'}},
            ui.row{gap = 24,
                ui.checkbox{id = 'music', text = {key = 'direction.music'}, checked = true},
                ui.toggle{id = 'vibration', text = {key = 'direction.vibration'}, checked = true},
            },
            ui.row{gap = 16,
                ui.label{text = {key = 'direction.volume'}, width = 200},
                ui.slider{id = 'volume', min = 0, max = 100, step = 1, value = 70, showValue = true, decimals = 0, grow = 1},
            },
            ui.formField{label = {key = 'direction.name'},
                ui.textField{id = 'name', placeholder = {key = 'direction.placeholder'}},
            },
            ui.list{id = 'inbox', items = {
                {id = 'inbox', text = {key = 'direction.inbox'}, caption = {key = 'direction.inboxCaption'}},
                {id = 'profile', text = {key = 'direction.profile'}, caption = {key = 'direction.profileCaption'}},
            }, selected = 'inbox'},
            ui.row{gap = 12,
                ui.button{id = 'save', text = {key = 'direction.save'}, variant = 'primary'},
                ui.button{id = 'cancel', text = {key = 'direction.cancel'}},
            },
            ui.label{text = {key = 'direction.note'}, color = 'textMuted'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'Directions of their own'},
            ui.label{id = 'state', text = '', font = 'monospace', color = 'accentText'},
            ui.label{text = "A column with \"direction = 'rightToLeft'\"", font = 'caption', color = 'textMuted'},
            ui.column{direction = 'rightToLeft', language = 'ar', gap = 12,
                ui.label{text = 'مرحبًا بك في الجزيرة! رقم الغرفة 42، والمفتاح (A-7).'},
                ui.textField{id = 'arabic', value = 'مرحبا 123 abc'},
                ui.row{gap = 12,
                    ui.checkbox{text = 'تذكرني', checked = true},
                    ui.button{text = 'دخول', variant = 'primary'},
                },
            },
            ui.label{text = "A column with \"direction = 'leftToRight'\" and \"language = 'hi'\"", font = 'caption', color = 'textMuted'},
            ui.column{direction = 'leftToRight', language = 'hi', gap = 12,
                ui.label{text = 'नमस्ते! क्षत्रिय, कृष्ण और हिन्दी के संयुक्ताक्षर।'},
                ui.textField{id = 'hindi', value = 'नमस्ते दुनिया'},
            },
        },
    }
end

-- The direction the UI draws in follows the language from the frame after it changes, so the state line reads it every frame and changes when it does.
function Direction:update(dt)
    Direction.super.update(self, dt)
    local set, drawn = ui.direction()
    local text = string.format('The call "localization.direction()" returns "%s".\nThe call "ui.direction()" returns "%s", drawing "%s".', localization.direction(), set, drawn)
    if text ~= self.state then
        self.state = text
        self:set('state', {text = text})
    end
end

return Direction
