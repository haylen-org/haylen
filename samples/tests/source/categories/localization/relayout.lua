-- Buttons measure their words, labels wrap at the width of their card and rows share what is left, so a new language lays the card out again without any code. The table on the right reads the bounds of the nodes every frame, the pseudo translation makes every text a third longer with accents to show where it would overflow, and the title shrinks to fit its width instead of ending with an ellipsis.
local haylen = require('haylen')
local localization = require('haylen.localization')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Relayout = haylen.class('Relayout', LanguageTest)

Relayout.code = [[
ui.button{id = 'play', text = {key = 'menu.play'}}  -- Measures again when the language changes.
ui.badge{id = 'hard', text = {key = 'layout.hard'}, tone = 'danger'}
localization.setPseudo(true)  -- Longer texts with accents, to test the room of every text.
local bounds = self.gui:bounds('play')]]

Relayout.hint = 'Pick another language, turn on the cycle or the pseudo translation, and watch the buttons and the paragraphs take their new sizes.'
Relayout.measured = {{'title', 'Title'}, {'intro', 'Paragraph'}, {'save', 'Save'}, {'cancel', 'Cancel'}, {'hard', 'Badge'}, {'chapter', 'Chapter'}}
Relayout.cycleSeconds = 1.5

function Relayout:content()
    return {
        ui.card{grow = 1, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 20,
                ui.label{id = 'title', text = {key = 'menu.title'}, font = 'heading', wrap = false, width = 420, minScale = 0.5},
                ui.label{id = 'intro', text = {key = 'layout.intro'}},
                ui.row{wrap = true, gap = 12,
                    ui.button{id = 'save', text = {key = 'layout.save'}, variant = 'primary'},
                    ui.button{id = 'cancel', text = {key = 'layout.cancel'}},
                    ui.button{id = 'options', text = {key = 'layout.options'}},
                },
                ui.row{gap = 12,
                    ui.label{text = {key = 'layout.difficulty'}},
                    ui.badge{id = 'hard', text = {key = 'layout.hard'}, tone = 'danger'},
                    ui.spacer{grow = 1},
                    ui.label{text = {key = 'extras.credits'}, color = 'textMuted'},
                },
                ui.divider{},
                ui.column{id = 'chapter', gap = 6,
                    ui.label{text = {key = 'story.chapters.second.title'}, font = 'heading'},
                    ui.label{text = {key = 'story.chapters.second.text'}, color = 'textMuted'},
                    ui.label{text = {key = 'story.characters.captain.line'}},
                },
            }},
            ui.row{gap = 32, margin = {12, 0, 0, 0},
                ui.toggle{id = 'cycle', text = 'Cycle the languages', onChange = function(event)
                    self:cycle(event.checked)
                end},
                ui.toggle{id = 'pseudo', text = 'Pseudo translation', onChange = function(event)
                    localization.setPseudo(event.checked)
                end},
            },
        },
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Sizes from "gui:bounds"'},
            ui.table{id = 'sizes', columns = {{text = 'Node'}, {text = 'Width', width = 150, align = 'end'}, {text = 'Height', width = 150, align = 'end'}}, rows = {}},
        },
    }
end

-- Shows the size every measured node was last drawn with, touching the table only when a size changed.
function Relayout:update(dt)
    Relayout.super.update(self, dt)
    local rows, key = {}, {}
    for _, node in ipairs(Relayout.measured) do
        local area = self.gui:bounds(node[1])
        if area then
            rows[#rows + 1] = {id = node[1], cells = {node[2], string.format('%.0f', area.width), string.format('%.0f', area.height)}}
            key[#key + 1] = string.format('%.0f %.0f', area.width, area.height)
        end
    end
    local sizes = table.concat(key, ' ')
    if sizes ~= self.sizes then
        self.sizes = sizes
        self:set('sizes', {rows = rows})
    end
end

function Relayout:exit()
    localization.setPseudo(false)
    Relayout.super.exit(self)
end

function Relayout:cycle(enabled)
    if enabled then
        self.timer = timer.every(Relayout.cycleSeconds, function()
            language.step(1)
        end, {owner = self})
    else
        timer.cancel(self.timer)
    end
end

return Relayout
