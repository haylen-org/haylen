-- The UI builds the elements that screen readers read while a screen reader asks for them, and the test turns the tree on with `accessibility.setTreeEnabled` so it shows them on every platform. The buttons of the panel act on the form the way a screen reader does, through `accessibility.perform`, and `accessibility.announce` asks the screen reader to say a text.
local accessibility = require('haylen.accessibility')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ScreenReader = haylen.class('ScreenReader', Test)

ScreenReader.code = [[
ui.slider{id = 'volume', value = 0.5, step = 0.1, accessibilityLabel = 'Volume'}
ui.icon{image = 'icons/star.png', accessibilityHidden = true}  -- Decoration that screen readers skip.
accessibility.setTreeEnabled(true)  -- Builds the elements even without a screen reader.
for _, element in ipairs(accessibility.elements()) do print(element.role, element.label, element.value) end
accessibility.perform(element.id, 'increment')  accessibility.announce('The profile was saved.')]]

ScreenReader.shown = 14

local function find(label)
    for _, element in ipairs(accessibility.elements()) do
        if element.label == label then
            return element
        end
    end
    return nil
end

-- The words a person reads for the states of an element.
ScreenReader.states = {focused = 'focused', checked = 'on', selected = 'selected', disabled = 'disabled', expanded = 'open'}
ScreenReader.roles = {group = 'Group', text = 'Text', heading = 'Heading', image = 'Image', button = 'Button', checkBox = 'Check box', switch = 'Switch', radioGroup = 'Radio group', radioButton = 'Radio', tabList = 'Tabs', tab = 'Tab', slider = 'Slider', textField = 'Field', comboBox = 'Combo', progressBar = 'Progress', list = 'List', dialog = 'Dialog', alert = 'Alert', playArea = 'Play area'}

-- The value of an element and its states, the way the last column of the table reads them.
local function describe(element)
    local parts = {}
    if element.value ~= '' then
        parts[1] = element.value
    end
    for _, state in ipairs({'focused', 'checked', 'selected', 'disabled', 'expanded'}) do
        if element[state] then
            parts[#parts + 1] = ScreenReader.states[state]
        end
    end
    return table.concat(parts, ', ')
end

function ScreenReader:enter()
    self.enabled = accessibility.treeEnabled()
    accessibility.setTreeEnabled(true)
    self.presses = 0
    self.listed = ''
    self:frame{
        stage = false,
        hint = 'With VoiceOver, TalkBack or a screen reader of the browser, move through the form and act on it, and the UI moves its focus along.',
        code = ScreenReader.code,
        content = {
            ui.card{gap = 16,
                ui.row{gap = 16, ui.icon{image = 'interface/themes/fantasy/icons/star.png', size = 56, accessibilityHidden = true}, ui.pageHeader{title = 'Audio', caption = 'How the game sounds', grow = 1}},
                ui.row{gap = 32, alignItems = 'start',
                    ui.column{grow = 1, gap = 16,
                        ui.button{id = 'play', text = 'Play a sound', onClick = function() self.presses = self.presses + 1 end},
                        ui.toggle{id = 'music', text = 'Music', checked = true},
                        ui.formField{label = 'Volume', ui.slider{id = 'volume', value = 0.5, step = 0.1, showValue = true, accessibilityLabel = 'Volume'}},
                    },
                    ui.column{grow = 1, gap = 16,
                        ui.formField{label = 'Quality', ui.radioGroup{id = 'quality', accessibilityLabel = 'Quality', horizontal = true, items = {{id = 'low', text = 'Low'}, {id = 'high', text = 'High'}}, selected = 'high'}},
                        ui.formField{label = 'Profile', ui.textField{id = 'name', placeholder = 'Name of the profile'}},
                        ui.row{gap = 16, ui.icon{image = 'interface/themes/fantasy/icons/heart.png', size = 48, accessibilityLabel = 'Favorite'}, ui.label{text = 'A favorite sound, whose icon a screen reader names', color = 'textMuted', grow = 1}},
                    },
                },
            },
        },
        controls = {
            ui.sectionTitle{text = 'Act like a screen reader'},
            ui.button{text = 'Activate "Play a sound"', onClick = function() self:act('Play a sound', 'activate') end},
            ui.button{text = 'Raise "Volume"', onClick = function() self:act('Volume', 'increment') end},
            ui.button{text = 'Pick "Low"', onClick = function() self:act('Low', 'activate') end},
            ui.button{text = 'Announce a text', onClick = function() accessibility.announce('The profile was saved.') end},
            ui.sectionTitle{text = 'Elements'},
            ui.row{gap = 16, ui.label{text = 'Role', font = 'caption', color = 'textMuted', width = 140}, ui.label{text = 'Label', font = 'caption', color = 'textMuted', grow = 1}, ui.label{text = 'Value and state', font = 'caption', color = 'textMuted', width = 170, textAlign = 'end'}},
            ui.column{id = 'elements', gap = 10, align = 'stretch'},
        },
        panelWidth = 720,
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

-- The list of elements builds again only when what they say changes, with every label whole on as many lines as it needs.
function ScreenReader:update(dt)
    ScreenReader.super.update(self, dt)
    local elements = accessibility.elements()
    local lines = {}
    for index = 1, math.min(#elements, ScreenReader.shown) do
        local element = elements[index]
        lines[index] = element.role .. element.label .. describe(element)
    end
    local listed = table.concat(lines, '|')
    if listed ~= self.listed then
        self.listed = listed
        local rows = {}
        for index = 1, math.min(#elements, ScreenReader.shown) do
            local element = elements[index]
            if index > 1 then
                rows[#rows + 1] = ui.divider{align = 'stretch'}
            end
            rows[#rows + 1] = ui.row{gap = 16, alignItems = 'start',
                ui.label{text = ScreenReader.roles[element.role] or element.role, width = 140},
                ui.label{text = element.label, grow = 1},
                ui.label{text = describe(element), color = 'textMuted', width = 170, textAlign = 'end'},
            }
        end
        self.gui:replaceChildren('elements', rows)
    end
    self:status(string.format('A screen reader reads %d elements, and "Play a sound" was pressed %d %s.', #elements, self.presses, self.presses == 1 and 'time' or 'times'))
end

return ScreenReader
