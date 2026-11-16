-- The home that the panel controls: the time of the day, which runs a minute every second, the rooms with their lights, the climate, the doors and the alarm, the sun on the roof, the battery and what the home uses, the blinds, the speaker and the washer, the scenes that set many of them at once and the events at the door. It changes on its own as the day goes on, and the panel only reads it and asks it to change.
local m = require('haylen.math')

local Home = {}
Home.__index = Home

-- One second of the app is one minute of the day, and the day starts at a quarter past seven in the morning.
Home.minutesPerSecond = 1
Home.startMinute = 7 * 60 + 15

-- The colors a light takes, by name, as the panel paints them.
Home.lightColors = {
    {id = 'warm', text = 'Warm', color = '#FFFFB547'},
    {id = 'daylight', text = 'Daylight', color = '#FFFFF1D6'},
    {id = 'cool', text = 'Cool', color = '#FFBFE3FF'},
    {id = 'violet', text = 'Violet', color = '#FFB79CFF'},
}

-- The doors that a scene locks: the patio stays open while the family is home in the day, and every door locks for the night and when everyone is away.
Home.dayLocks = {front = true, garage = true, patio = false}
Home.allLocks = {front = true, garage = true, patio = true}

-- What each scene sets: the brightness of every room and the color of its lights, the blinds, the alarm, the locks of the doors, the target and the mode of the climate and the music. The panel opens with the scene of the morning on.
Home.scenes = {
    morning = {text = 'Morning', icon = 'sun', rooms = {living = 70, kitchen = 90, bedroom = 40, office = 0}, color = 'daylight', blinds = 100, alarm = 'off', locks = Home.dayLocks, target = 21.5, mode = 'heat', music = true},
    away = {text = 'Away', icon = 'lock', rooms = {living = 0, kitchen = 0, bedroom = 0, office = 0}, color = 'warm', blinds = 30, alarm = 'away', locks = Home.allLocks, target = 17, mode = 'auto', music = false},
    movie = {text = 'Movie', icon = 'film', rooms = {living = 15, kitchen = 0, bedroom = 0, office = 0}, color = 'violet', blinds = 0, alarm = 'off', locks = Home.dayLocks, target = 22, mode = 'heat', music = false},
    night = {text = 'Night', icon = 'moon', rooms = {living = 0, kitchen = 0, bedroom = 10, office = 0}, color = 'warm', blinds = 0, alarm = 'home', locks = Home.allLocks, target = 19, mode = 'auto', music = false},
}
Home.sceneOrder = {'morning', 'away', 'movie', 'night'}
Home.startScene = 'morning'

-- The events at the door that arrive now and then, as a notice and on the camera.
Home.events = {
    {kind = 'doorbell', text = 'Someone rang the doorbell at the front door.'},
    {kind = 'motion', text = 'The camera saw motion on the front path.'},
    {kind = 'parcel', text = 'A parcel was left at the front door.'},
}
Home.eventInterval = 25

function Home.new()
    local self = setmetatable({}, Home)
    self.minute = Home.startMinute
    self.time = 0
    self.rooms = {
        {id = 'living', text = 'Living room', icon = 'sofa', lights = 4},
        {id = 'kitchen', text = 'Kitchen', icon = 'kitchen', lights = 3},
        {id = 'bedroom', text = 'Bedroom', icon = 'bed', lights = 2},
        {id = 'office', text = 'Office', icon = 'desk', lights = 2},
    }
    self.climate = {current = 20.6, humidity = 44}
    self.locks = {
        {id = 'front', text = 'Front door'},
        {id = 'garage', text = 'Garage'},
        {id = 'patio', text = 'Patio door'},
    }
    self.speaker = {volume = 40, track = 'Morning Light', artist = 'The Quiet Hours'}
    self.washer = {running = true, total = 95, left = 41}
    self.battery = 0.58
    self:setScene(Home.startScene)
    self.usage = {}
    self.solarCurve = {}
    for slot = 1, 96 do
        self.usage[slot] = Home.usualUsage((slot - 1) * 15)
        self.solarCurve[slot] = Home.solarAt((slot - 1) * 15)
    end
    self.nextEvent = 8
    self.motion = 0
    self.listeners = {}
    return self
end

-- The power of the panels on the roof in kilowatts for a minute of the day: nothing at night and up to 6 kW at noon.
function Home.solarAt(minute)
    local daylight = (minute - 6 * 60) / (14 * 60)
    if daylight <= 0 or daylight >= 1 then
        return 0
    end
    return 6 * math.sin(daylight * math.pi) ^ 1.6
end

-- The power a home like this one uses at a minute of the day, in kilowatts, which fills the hours of the chart before the app started: a quiet night, a morning and an evening peak.
function Home.usualUsage(minute)
    local hour = minute / 60
    local morning = 1.6 * math.exp(-((hour - 7.5) / 1.2) ^ 2)
    local evening = 2.4 * math.exp(-((hour - 19.5) / 1.8) ^ 2)
    return 0.4 + morning + evening
end

-- The share of the day from 0 at midnight to 1 at the next one.
function Home:dayShare()
    return (self.minute % (24 * 60)) / (24 * 60)
end

-- The power the home uses in kilowatts: a base, the lights, the climate working toward its target, the washer and the speaker.
function Home:consumption()
    local lights = 0
    for _, room in ipairs(self.rooms) do
        if room.on then
            lights = lights + room.lights * 0.009 * room.level / 100
        end
    end
    local climate = self.climate.mode == 'off' and 0 or math.min(2.4, math.abs(self.climate.target - self.climate.current) * 1.6)
    local washer = self.washer.running and 0.8 + 0.6 * math.max(0, math.sin(self.time * 0.9)) or 0
    local speaker = self.speaker.playing and 0.03 or 0
    return 0.35 + lights + climate + washer + speaker
end

function Home:solar()
    return Home.solarAt(self.minute % (24 * 60))
end

-- What the battery gives, positive, or takes, negative, in kilowatts, and what the grid gives once the battery is empty or full.
function Home:balance()
    local surplus = self:solar() - self:consumption()
    if surplus > 0 then
        local charging = self.battery < 1 and math.min(surplus, 3.5) or 0
        return -charging, -(surplus - charging)
    end
    local giving = self.battery > 0.08 and math.min(-surplus, 3.5) or 0
    return giving, -surplus - giving
end

function Home:update(dt)
    self.time = self.time + dt
    local minutes = dt * Home.minutesPerSecond
    self.minute = self.minute + minutes

    -- The battery of 13.5 kWh follows the balance of the sun and the home.
    local battery = self:balance()
    self.battery = m.clamp(self.battery - battery * minutes / 60 / 13.5, 0, 1)

    -- The room warms or cools toward the target, and the air dries a little while the heating runs.
    local climate = self.climate
    if climate.mode ~= 'off' then
        local direction = climate.target - climate.current
        local allowed = (climate.mode == 'heat' and direction > 0) or (climate.mode == 'cool' and direction < 0) or climate.mode == 'auto'
        if allowed then
            climate.current = climate.current + m.clamp(direction, -1, 1) * minutes * 0.02
        end
    end
    climate.current = climate.current + (16 + 6 * math.sin((self:dayShare() - 0.3) * math.pi * 2) - climate.current) * minutes * 0.001
    climate.humidity = 44 + 5 * math.sin(self.time * 0.05)

    -- The washer counts down and stops at the end of its program.
    local washer = self.washer
    if washer.running then
        washer.left = washer.left - minutes
        if washer.left <= 0 then
            washer.left, washer.running = 0, false
            self:emit('notice', 'The washer finished its program.')
        end
    end

    -- The usage of the day fills its slot of a quarter of an hour.
    local slot = math.floor((self.minute % (24 * 60)) / 15) + 1
    self.usage[slot] = self:consumption()

    self.motion = math.max(0, self.motion - dt)
    self.nextEvent = self.nextEvent - dt
    if self.nextEvent <= 0 then
        self.nextEvent = Home.eventInterval
        self.eventIndex = (self.eventIndex or 0) % #Home.events + 1
        self.motion = 6
        self:emit('notice', Home.events[self.eventIndex].text)
    end
end

-- Calls the listeners of a kind of change, such as a notice or a change of state that the controls show.
function Home:on(kind, listener)
    self.listeners[kind] = listener
end

function Home:emit(kind, value)
    local listener = self.listeners[kind]
    if listener then
        listener(value)
    end
end

-- Sets everything a scene holds: the lights of every room and their color, the blinds, the alarm, the locks, the climate and the music.
function Home:setScene(id)
    local scene = Home.scenes[id]
    self.scene = id
    for _, room in ipairs(self.rooms) do
        room.level = scene.rooms[room.id]
        room.on = room.level > 0
        room.color = scene.color
    end
    for _, lock in ipairs(self.locks) do
        lock.locked = scene.locks[lock.id]
    end
    self.blinds = scene.blinds
    self.alarm = scene.alarm
    self.climate.target = scene.target
    self.climate.mode = scene.mode
    self.speaker.playing = scene.music
end

-- Applies a scene that the person chose, and tells the panel and the person.
function Home:applyScene(id)
    self:setScene(id)
    self:emit('changed')
    self:emit('notice', 'The ' .. string.lower(Home.scenes[id].text) .. ' scene is on.')
end

function Home:room(id)
    for _, room in ipairs(self.rooms) do
        if room.id == id then
            return room
        end
    end
    error('There is no room with the id "' .. id .. '".')
end

function Home:lightColor(id)
    for _, entry in ipairs(Home.lightColors) do
        if entry.id == id then
            return entry
        end
    end
    error('There is no light color with the id "' .. id .. '".')
end

-- The number of lights on and of doors unlocked.
function Home:lightsOn()
    local count = 0
    for _, room in ipairs(self.rooms) do
        if room.on then
            count = count + room.lights
        end
    end
    return count
end

function Home:unlockedDoors()
    local count = 0
    for _, lock in ipairs(self.locks) do
        if not lock.locked then
            count = count + 1
        end
    end
    return count
end

return Home
