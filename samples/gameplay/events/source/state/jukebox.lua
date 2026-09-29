-- A jukebox that the autoloads test adds at run time with haylen.autoload. It counts the beats of its track for as long as the app runs, also while the game is paused.
local jukebox = {track = 'Island theme', tempo = 120, beats = 0, time = 0, processMode = 'always'}

function jukebox:update(dt)
    self.time = self.time + dt
    self.beats = math.floor(self.time * self.tempo / 60)
end

return jukebox
