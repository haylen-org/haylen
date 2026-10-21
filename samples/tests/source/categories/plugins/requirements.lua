-- The plugin calls a method that needs something the project of the app lacks on purpose, such as a permission that the Android manifest does not declare. The native part checks the requirement before it calls the system, logs once what is missing and how to add it, and fails the call with the code `unsupported`, whose `data.missing` lists each missing requirement with the file and the snippet that add it, instead of crashing the app.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Requirements = haylen.class('Requirements', DemoTest)

-- What the project of each platform leaves out on purpose, which the call reports.
Requirements.expected = {
    android = {'The permission "android.permission.READ_CONTACTS"', 'The manifest of the app does not declare it.'},
    ios = {'The usage description "NSContactsUsageDescription"', 'The "Info.plist" of the app does not have it.'},
    tvos = {'The usage description "NSContactsUsageDescription"', 'The "Info.plist" of the app does not have it.'},
    macos = {'The usage description "NSContactsUsageDescription"', 'The "Info.plist" of the app does not have it.'},
    web = {'The Contact Picker API "navigator.contacts"', 'Only the browsers of phones offer it to pages.'},
}

-- The platforms in the order the card lists what each one lacks.
Requirements.platforms = {{'android', 'Android'}, {'ios', 'Apple'}, {'web', 'Web'}}

-- The words for the kinds of requirements that "data.missing" names.
Requirements.kinds = {api = 'API', permission = 'permission', class = 'class', feature = 'feature', secureContext = 'secure context'}

function Requirements:enter()
    local expected = Requirements.expected[haylen.platform] or {'Nothing', 'The native part of this platform checks no requirements.'}
    local platforms = {}
    for index, platform in ipairs(Requirements.platforms) do
        local lacks = Requirements.expected[platform[1]]
        platforms[index] = ui.row{gap = 12, alignItems = 'start',
            ui.badge{text = platform[2], tone = 'accent', width = 110},
            ui.column{grow = 1, gap = 2, ui.label{text = lacks[1]}, ui.label{text = lacks[2], font = 'caption', color = 'textMuted'}},
        }
    end
    self:frame{
        hint = 'The test checks the requirement when it opens. Check it again once the project adds the snippet the call names.',
        focus = 'check',
        checksEmpty = 'Check the requirement to see the call fail without crashing the app.',
        controls = {
            ui.button{id = 'check', text = 'Check it again', variant = 'primary', onClick = function() self:check() end},
            ui.label{text = 'The call fails with the code "unsupported" instead of crashing the app, and the log tells once what is missing and how to add it.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 16,
                ui.label{text = 'What the project lacks', font = 'heading'},
                ui.column{id = 'missing', gap = 12, align = 'stretch',
                    ui.alert{tone = 'information', title = expected[1], message = expected[2] .. ' The call names the file and the snippet that add it.'},
                },
                ui.divider{},
                ui.label{text = 'What each platform lacks on purpose', font = 'caption', color = 'textMuted'},
                ui.scroll{grow = 1, height = 0, ui.column{gap = 14, children = platforms}},
            },
        },
        code = "local _, err = demo.requirementCheck():await()\nfor _, missing in ipairs(err.data.missing) do\n  print(missing.file, missing.snippet)\nend",
    }
    self:check()
end

function Requirements:check()
    self:act(function()
        self.results:clear()
        local name = 'The call "requirementCheck"'
        self.results:set('call', 'waiting', name, 'The native part checks what the project of the app holds.')
        local answer, err = demo.requirementCheck():await()
        if not err then
            self.gui:replaceChildren('missing', {ui.alert{tone = 'success', title = 'Nothing is missing', message = 'The project has what the native part needs.'}})
            self.results:set('call', 'pass', name, string.format('The project has what the native part in %s needs, so the call answered.', answer.language))
            return
        elseif err.code == 'noHandler' then
            self.results:set('call', 'skip', name, 'The native part of this platform checks no requirements.')
            return
        elseif err.code ~= 'unsupported' or not (err.data and err.data.missing) then
            self.results:failure('call', name, err)
            return
        end

        self.results:set('call', 'pass', name, 'The call failed with the code "unsupported": ' .. err.message)
        -- A requirement of the web page has no file of a project, and only a feature of the permissions policy has a snippet that allows it.
        local alerts = {}
        for index, missing in ipairs(err.data.missing) do
            local detail = 'The page lacks it, as the log explains.'
            if missing.file ~= '' then
                detail = string.format('Add "%s" to "%s".', missing.snippet, missing.file)
            elseif missing.snippet ~= '' then
                detail = string.format('Allow it with "%s".', missing.snippet)
            end
            alerts[index] = ui.alert{tone = 'warning', title = string.format('The %s "%s" is missing', Requirements.kinds[missing.kind] or missing.kind, missing.name), message = detail}
        end
        self.gui:replaceChildren('missing', alerts)
    end)
end

return Requirements
