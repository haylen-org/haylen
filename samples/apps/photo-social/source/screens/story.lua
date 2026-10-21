-- The stories, full screen: every picture of a person shows for five seconds under bars that fill as it plays, then the next one, then the stories of the next person. A tap on the left third goes back and anywhere else goes on, a press holds the story, the arrows, the directional pad and a remote step through it while it has the focus, and the field at the bottom replies in the direct messages. A phone shows the pictures from edge to edge, and a wide window shows them in a tall frame with arrows beside it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Chats = require('chats')
local Page = require('page')
local Shell = require('shell')
local Social = require('data.social')
local Style = require('style')

local Story = haylen.class('Story', Page)

Story.duration = 5
Story.tapTime = 0.3
Story.backTransition = {duration = 0.3, effect = 'push', direction = 'down', ease = 'cubicOut'}
Story.white = '#FFFFFFFF'
Story.field = {colors = {raised = '#26FFFFFF', border = '#99FFFFFF', borderStrong = '#FFFFFFFF', text = '#FFFFFFFF', textMuted = '#D9FFFFFF', caret = '#FFFFFFFF', focus = '#FFFFFFFF'}, metrics = {controlRadius = Style.radius.field}}
Story.onPhoto = {colors = {text = '#FFFFFFFF', hover = '#26FFFFFF', pressed = '#40FFFFFF'}}

function Story:enter()
    self.storyIndex = self.params.index
    self.frame, self.elapsed = 1, 0
    self:mountGui(ui.stack{id = 'root', onCancel = Shell.back}, {placement = 'screen'})
end

function Story:layoutChanged(wide)
    self.gui:replaceChildren('root', {self:stage(wide), self:closeButton(wide)})
    self:showStory()
    self.gui:command('area', 'focus')
end

function Story:story()
    return Social.stories[self.storyIndex]
end

-- The picture with its overlays, from edge to edge on a phone or in a tall frame between two arrows on a wide window.
function Story:stage(wide)
    local space = Style.space
    local frame = ui.stack{id = 'frame', align = 'stretch', grow = 1,
        ui.image{id = 'photo', image = Social.stories[1].frames[1].image, fit = 'cover', align = 'stretch', radius = wide and Style.radius.card or 0, accessibilityLabel = 'Story',
            onPress = function()
                self.held, self.pressedAt = true, haylen.elapsed()
            end,
            onRelease = function(event)
                self.held = false
                if event.inside and haylen.elapsed() - self.pressedAt < Story.tapTime then
                    local bounds = self.gui:bounds('photo')
                    if event.x < bounds.x + bounds.width / 3 then
                        self:previous()
                    else
                        self:next()
                    end
                end
            end},
        -- Dark fades at the top and the bottom keep the text over the picture readable.
        ui.column{align = 'stretch', gap = 0,
            ui.image{image = 'images/scrim-top.svg', fit = 'fill', height = 240, align = 'stretch', radius = wide and Style.radius.card or 0, accessibilityHidden = true},
            ui.spacer{grow = 1},
            ui.image{image = 'images/scrim-bottom.svg', fit = 'fill', height = 300, align = 'stretch', radius = wide and Style.radius.card or 0, accessibilityHidden = true},
        },
        ui.playArea{id = 'area', align = 'stretch', focusRing = false},
        self:overlay(wide),
    }
    if not wide then
        return ui.row{anchor = 'stretch', anchorTo = 'screen', gap = 0, frame}
    end
    frame.grow = 0
    frame.aspectRatio = 9 / 16
    frame.height = self:frameHeight()
    return ui.row{anchor = 'stretch', gap = space.xl, padding = {space.l, 0},
        ui.spacer{grow = 1},
        self:arrow('previous', 'back', 'Previous story'),
        frame,
        self:arrow('next', 'next', 'Next story'),
        ui.spacer{grow = 1},
    }
end

-- The height of the tall frame of a wide window: the height of the safe area less a margin above and below, from which the frame takes its width.
function Story:frameHeight()
    local _, unit = ui.scale()
    return math.floor(viewport.safeRect().height / unit - Style.space.l * 2)
end

function Story:arrow(id, icon, label)
    return ui.button{id = id, icon = Shell.icon(icon), iconColor = 'text', width = 64, height = 64, accessibilityLabel = label, tooltip = label, style = {metrics = {controlRadius = 32, controlPaddingX = 0}}, onClick = function()
        self[id](self)
    end}
end

function Story:closeButton(wide)
    if not wide then
        return ui.spacer{}
    end
    return ui.button{id = 'closeWide', icon = Shell.icon('close'), variant = 'icon', anchor = 'topRight', margin = Style.space.l, accessibilityLabel = 'Close', tooltip = 'Close', style = Story.onPhoto, onClick = Shell.back}
end

-- The bars, the person, the note of the picture and the reply, kept inside the safe area of a phone.
function Story:overlay(wide)
    local space = Style.space
    local column = ui.column{align = 'stretch', gap = space.s, padding = wide and {space.m, space.m, space.m, space.m} or {space.s, space.m, space.m, space.m},
        ui.row{id = 'bars', gap = space.xxs},
        ui.row{gap = space.s,
            ui.avatar{id = 'avatar', image = Social.me.avatar, size = Style.size.avatarSmall},
            ui.column{gap = 0, grow = 1,
                ui.label{id = 'name', text = '', font = 'button', color = 'onAccent', wrap = false},
                ui.label{id = 'age', text = '', font = 'caption', color = 'onAccent', wrap = false},
            },
            ui.button{id = 'close', icon = Shell.icon('close'), variant = 'icon', visible = not wide, accessibilityLabel = 'Close', tooltip = 'Close', style = Story.onPhoto, onClick = Shell.back},
        },
        ui.spacer{grow = 1},
        ui.label{id = 'note', text = '', font = 'title', color = 'onAccent', outline = '#73000000', outlineWidth = 3, textAlign = 'center', align = 'stretch'},
        ui.spacer{grow = 1},
        ui.row{gap = space.xs,
            ui.textField{id = 'reply', placeholder = 'Send a message', grow = 1, returnKey = 'send', style = Story.field,
                onFocus = function()
                    self.typing = true
                end,
                onBlur = function()
                    self.typing = false
                end,
                onChange = function(event)
                    self.draft = event.value
                end,
                onSubmit = function(event)
                    self:reply(event.value)
                end},
            ui.button{id = 'like', icon = Shell.icon('heart'), variant = 'icon', accessibilityLabel = 'Like', tooltip = 'Like', style = Story.onPhoto, onClick = function()
                self:toggleLike()
            end},
            ui.button{id = 'send', icon = Shell.icon('send'), variant = 'icon', accessibilityLabel = 'Send', tooltip = 'Send', style = Story.onPhoto, onClick = function()
                self:reply(self.draft or '')
            end},
        },
    }
    if wide then
        return column
    end
    return ui.safeArea{align = 'stretch', column}
end

-- Shows the person whose stories play: the bars, the face, the name and the first picture.
function Story:showStory()
    local story = self:story()
    local person = Social.person(story.person)
    story.seen = true
    local bars = {}
    for index = 1, #story.frames do
        bars[index] = ui.progress{id = 'bar-' .. index, value = 0, grow = 1, accessibilityHidden = true, style = {metrics = {progressHeight = 5}, colors = {track = '#59FFFFFF', accent = Story.white}}}
    end
    self.gui:replaceChildren('bars', bars)
    self.gui:set('avatar', {image = person.avatar})
    self.gui:set('name', {text = person.handle})
    self.gui:set('age', {text = story.age .. ' ago'})
    self.gui:set('reply', {placeholder = 'Reply to ' .. person.handle})
    self.liked = false
    self.gui:set('like', {icon = Shell.icon('heart')})
    self:showFrame()
end

function Story:showFrame()
    local story = self:story()
    local frame = story.frames[self.frame]
    self.elapsed = 0
    self.shownValue = nil
    self.gui:set('photo', {image = frame.image})
    self.gui:set('note', {text = frame.note})
    for index = 1, #story.frames do
        self.gui:set('bar-' .. index, {value = index < self.frame and 1 or 0})
    end
end

function Story:next()
    local story = self:story()
    if self.frame < #story.frames then
        self.frame = self.frame + 1
        self:showFrame()
    elseif self.storyIndex < #Social.stories then
        self.storyIndex, self.frame = self.storyIndex + 1, 1
        self:showStory()
    else
        Shell.back()
    end
end

function Story:previous()
    if self.frame > 1 then
        self.frame = self.frame - 1
        self:showFrame()
    elseif self.storyIndex > 1 then
        self.storyIndex, self.frame = self.storyIndex - 1, 1
        self:showStory()
    else
        self:showFrame()
    end
end

function Story:toggleLike()
    self.liked = not self.liked
    self.gui:set('like', {icon = Shell.icon(self.liked and 'heart-filled' or 'heart')})
end

function Story:reply(text)
    if text:match('^%s*$') then
        return
    end
    local person = Social.person(self:story().person)
    Chats.send(Chats.with(person), text)
    self.draft = ''
    self.gui:set('reply', {value = ''})
    Shell.notify('Sent to ' .. person.handle .. '.')
end

function Story:update(dt)
    Page.update(self, dt)
    if self.wide then
        local height = self:frameHeight()
        if self.frameShown ~= height then
            self.frameShown = height
            self.gui:set('frame', {height = height})
        end
    end
    if ui.focusOwner() == 'playArea' then
        if input.pressed('storyNext') then
            self:next()
        elseif input.pressed('storyPrevious') then
            self:previous()
        end
    end
    if self.held or self.typing or self.exiting then
        return
    end
    self.elapsed = self.elapsed + dt
    if self.elapsed >= Story.duration then
        self:next()
        return
    end
    local value = math.floor(self.elapsed / Story.duration * 200) / 200
    if self.shownValue ~= value then
        self.shownValue = value
        self.gui:set('bar-' .. self.frame, {value = value})
    end
end

function Story:exitTransitionStarted()
    self.exiting = true
end

function Story:render()
    graphics2d.beginScreen()
    graphics2d.drawRect(viewport.visibleRect(), '#FF101014')
end

return Story
