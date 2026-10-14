-- The UI builds the elements that screen readers read while a screen reader asks for them, and the test turns the tree on with `accessibility.setTreeEnabled` so it shows them on every platform. The buttons of the panel act on the form the way a screen reader does, through `accessibility.perform`, and `accessibility.announce` asks the screen reader to say a text.
local accessibility = require('haylen.accessibility')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ScreenReader = haylen.class('ScreenReader', Test)

ScreenReader.shown = 14

local function find(label)
    for _, element in ipairs(accessibility.elements()) do
        if element.label == label then
            return element
        end
    end
    return nil
end

local function describe(element)
    local states = {}
    for _, state in ipairs({'focused', 'checked', 'selected', 'disabled', 'expanded'}) do
        if element[state] then
            states[#states + 1] = state
        end
    end
    local value = element.value ~= '' and ' = ' .. element.value or ''
    return string.format('%-11s %s%s %s', element.role, element.label, value, table.concat(states, ' '))
end

function ScreenReader:enter()
    self.enabled = accessibility.treeEnabled()
    accessibility.setTreeEnabled(true)
    self.presses = 0
    self:frame{
        stage = false,
        hint = 'With VoiceOver, TalkBack or a screen reader of the browser, move through the form and act on it, and the UI moves its focus along.',
        content = {
            ui.panel{gap = 12,
                ui.pageHeader{title = 'Audio', caption = 'How the game sounds'},
                ui.button{id = 'play', text = 'Play a sound', onClick = function() self.presses = self.presses + 1 end},
                ui.toggle{id = 'music', text = 'Music', checked = true},
                ui.slider{id = 'volume', value = 0.5, step = 0.1, accessibilityLabel = 'Volume'},
                ui.radioGroup{id = 'quality', accessibilityLabel = 'Quality', horizontal = true, items = {{id = 'low', text = 'Low'}, {id = 'high', text = 'High'}}, selected = 'high'},
                ui.textField{id = 'name', placeholder = 'Name of the profile'},
                ui.icon{image = 'interface/icons/heart.png', accessibilityLabel = 'Favorite'},
                ui.icon{image = 'interface/icons/star.png', size = 24, color = 'textMuted', accessibilityHidden = true},
            },
        },
        controls = {
            ui.sectionTitle{text = 'Act like a screen reader'},
            ui.button{text = 'Activate "Play a sound"', onClick = function() self:act('Play a sound', 'activate') end},
            ui.button{text = 'Raise "Volume"', onClick = function() self:act('Volume', 'increment') end},
            ui.button{text = 'Pick "Low"', onClick = function() self:act('Low', 'activate') end},
            ui.button{text = 'Announce a text', onClick = function() accessibility.announce('The profile was saved.') end},
            ui.sectionTitle{text = 'Elements'},
            ui.label{id = 'elements', text = '', font = 'monospace'},
        },
        panelWidth = 640,
    }
end

function ScreenReader:act(label, action)
    local element = find(label)
    if element then
        accessibility.perform(element.id, action)
    end
end

function ScreenReader:exit()
    accessibility.setTreeEnabled(self.enabled)
    ScreenReader.super.exit(self)
end

function ScreenReader:update(dt)
    ScreenReader.super.update(self, dt)
    local elements = accessibility.elements()
    local lines = {}
    for index = 1, math.min(#elements, ScreenReader.shown) do
        lines[index] = describe(elements[index])
    end
    self:set('elements', {text = table.concat(lines, '\n')})
    self:status(string.format('%d elements, %d presses of "Play a sound".', #elements, self.presses))
end

return ScreenReader
