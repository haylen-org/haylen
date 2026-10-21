-- The last step of a new post: the picture beside the caption, the place, and the button that shares it to the top of the feed and the profile.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Compose = require('compose')
local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')

local ComposeShare = haylen.class('ComposeShare', Page)

function ComposeShare:enter()
    local space = Style.space
    self.caption, self.place = '', ''
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('New post', {
            ui.button{id = 'shareTop', text = 'Share', variant = 'link', onClick = function()
                self:share()
            end},
        }),
        ui.scroll{grow = 1, height = 0,
            Shell.centered(720, ui.column{gap = space.l, padding = {space.l, space.m, space.xl, space.m},
                ui.image{id = 'thumb', image = Compose.texture and Compose.preview or Compose.look(), fit = 'cover', aspectRatio = 1, maxWidth = 360, align = 'center', radius = Style.radius.photo, accessibilityLabel = 'The picture of the post'},
                ui.formField{label = 'Caption',
                    ui.textArea{id = 'caption', placeholder = 'Write a caption', maxLength = 200, rows = 3, onChange = function(event)
                        self.caption = event.value
                    end},
                },
                ui.formField{label = 'Place', help = 'Where the picture was taken.',
                    ui.textField{id = 'place', placeholder = 'Add a place', maxLength = 40, autocapitalize = 'words', returnKey = 'done', onChange = function(event)
                        self.place = event.value
                    end},
                },
                ui.button{id = 'share', text = 'Share', variant = 'primary', align = 'stretch', onClick = function()
                    self:share()
                end},
            }),
        },
    })
end

function ComposeShare:share()
    local post = Social.publish(Compose.finish(), self.caption:match('^%s*(.-)%s*$'), self.place:match('^%s*(.-)%s*$'))
    Shell.showTab('home')
    Shell.notify('Your post is shared.', 'success')
    return post
end

return ComposeShare
