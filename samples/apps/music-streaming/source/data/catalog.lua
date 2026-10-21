-- The made-up music of the app: genres, artists, albums with covers drawn from shapes, their songs, the liked songs, the mixes and the playlists. Every value comes from a seeded generator, so the same library shows on every run.
local events = require('haylen.events')

local Random = require('random')
local Synth = require('synth')

local Catalog = {}

Catalog.artistCount = 120
Catalog.likedCount = 2400

Catalog.genres = {
    {id = 'pop', name = 'Pop', style = 'pop', colors = {'#FFFF6B9A', '#FFFFC93C'}},
    {id = 'electronic', name = 'Electronic', style = 'electronic', colors = {'#FF2FD6C9', '#FF5B4BFF'}},
    {id = 'lofi', name = 'Lo-fi beats', style = 'lofi', colors = {'#FFB38BFA', '#FFF7A072'}},
    {id = 'acoustic', name = 'Acoustic', style = 'acoustic', colors = {'#FFF2B45A', '#FF7BC74D'}},
    {id = 'ambient', name = 'Ambient', style = 'ambient', colors = {'#FF7FB8E6', '#FFC6F0E8'}},
    {id = 'rock', name = 'Rock', style = 'rock', colors = {'#FFE8452C', '#FF2B2E4A'}},
    {id = 'jazz', name = 'Jazz', style = 'jazz', colors = {'#FFF2C14E', '#FF3E5A96'}},
    {id = 'funk', name = 'Funk', style = 'funk', colors = {'#FFFF8A3D', '#FF9B6BFF'}},
}

Catalog.firstWords = {'Lumen', 'Copper', 'Velvet', 'Paper', 'Nova', 'Echo', 'Saffron', 'Indigo', 'Hollow', 'Amber', 'Cobalt', 'Willow', 'Marble', 'Static', 'Quiet', 'Lantern', 'Harbor', 'Juniper', 'Neon', 'Opal'}
Catalog.secondWords = {'Atlas', 'Tide', 'Orchard', 'Signals', 'Coast', 'Parade', 'Gardens', 'Engines', 'Comets', 'Rivers', 'Moths', 'Satellites', 'Foxes', 'Kites'}
Catalog.titleWords = {'Midnight', 'Golden', 'Slow', 'Paper', 'Electric', 'Summer', 'Silver', 'Falling', 'Little', 'Endless', 'Hidden', 'Bright', 'Gentle', 'Wild', 'Late'}
Catalog.titleNouns = {'Lights', 'Waves', 'Hours', 'Streets', 'Dreams', 'Skies', 'Roads', 'Rooms', 'Echoes', 'Gardens', 'Signals', 'Mornings', 'Rivers', 'Hearts', 'Stars'}

Catalog.playlists = {
    {id = 'liked', name = 'Liked songs', description = 'Every song you liked, newest first.'},
    {id = 'drive', name = 'Night drive', description = 'Neon lights and empty roads.'},
    {id = 'focus', name = 'Deep focus', description = 'Calm music to keep you in the zone.'},
    {id = 'coffee', name = 'Morning coffee', description = 'Easy songs for slow starts.'},
    {id = 'workout', name = 'Workout boost', description = 'Fast songs that keep you moving.'},
    {id = 'rain', name = 'Rainy window', description = 'Soft sounds for gray days.'},
}

-- The colors of the covers, which the shapes of a cover combine.
Catalog.palette = {'#FFFF6B9A', '#FFFFC93C', '#FF2FD6C9', '#FF5B4BFF', '#FFB38BFA', '#FFF7A072', '#FF7BC74D', '#FF7FB8E6', '#FFE8452C', '#FF3E5A96', '#FFFF8A3D', '#FF9B6BFF', '#FFFFF4E0', '#FF2B2E4A'}

function Catalog.duration(seconds)
    return string.format('%d:%02d', seconds // 60, math.floor(seconds % 60))
end

function Catalog.genre(id)
    return Catalog.genreById[id]
end

function Catalog.build()
    local random = Random(83)
    Catalog.genreById = {}
    for _, genre in ipairs(Catalog.genres) do
        Catalog.genreById[genre.id] = genre
    end
    Catalog.artists, Catalog.albums, Catalog.tracks = {}, {}, {}
    Catalog.byId = {}
    local taken = {}
    for index = 1, Catalog.artistCount do
        local name
        repeat
            name = random:pick(Catalog.firstWords) .. ' ' .. random:pick(Catalog.secondWords)
        until not taken[name]
        taken[name] = true
        local genre = random:pick(Catalog.genres)
        local artist = {id = 'artist-' .. index, type = 'artist', name = name, genre = genre.id, listeners = random:integer(3, 900) * 1000, albums = {}, tracks = {}}
        artist.caption = string.format('%s · %s monthly listeners', genre.name, Catalog.thousands(artist.listeners))
        Catalog.artists[index] = artist
        Catalog.byId[artist.id] = artist
        for album = 1, 3 do
            Catalog.addAlbum(random, artist, genre)
        end
        artist.cover = artist.albums[1].cover
    end
    Catalog.liked = {}
    for index = 1, Catalog.likedCount do
        local track = Catalog.tracks[(index * 7919) % #Catalog.tracks + 1]
        if not track.liked then
            track.liked = true
            Catalog.liked[#Catalog.liked + 1] = track
        end
    end
    -- The other playlists take songs of the genres that suit them.
    local suits = {drive = {'electronic', 'pop', 'rock'}, focus = {'ambient', 'lofi'}, coffee = {'acoustic', 'jazz'}, workout = {'electronic', 'rock', 'funk'}, rain = {'lofi', 'ambient', 'acoustic'}}
    for _, playlist in ipairs(Catalog.playlists) do
        playlist.type = 'playlist'
        Catalog.byId[playlist.id] = playlist
        if playlist.id == 'liked' then
            playlist.tracks = Catalog.liked
        else
            playlist.tracks = {}
            for _, track in ipairs(Catalog.tracks) do
                for _, genre in ipairs(suits[playlist.id]) do
                    if track.genre == genre and random:chance(0.06) then
                        playlist.tracks[#playlist.tracks + 1] = track
                    end
                end
            end
        end
        playlist.cover = playlist.tracks[1].cover
        playlist.caption = string.format('Playlist · %s songs', Catalog.thousands(#playlist.tracks))
    end
    Catalog.mixes = {}
    for index, genre in ipairs(Catalog.genres) do
        local mix = {id = 'mix-' .. genre.id, type = 'playlist', genre = genre.id, name = 'Daily mix ' .. index, description = 'The ' .. string.lower(genre.name) .. ' you play most, and more like it.', tracks = {}}
        for _, track in ipairs(Catalog.tracks) do
            if track.genre == genre.id and #mix.tracks < 40 then
                mix.tracks[#mix.tracks + 1] = track
            end
        end
        mix.cover = Catalog.newCover('mix-' .. genre.id, random, genre)
        mix.caption = genre.name .. ' mix'
        Catalog.mixes[index] = mix
        Catalog.byId[mix.id] = mix
    end
end

function Catalog.addAlbum(random, artist, genre)
    local index = #Catalog.albums + 1
    local album = {
        id = 'album-' .. index,
        type = 'album',
        name = random:pick(Catalog.titleWords) .. ' ' .. random:pick(Catalog.titleNouns),
        artist = artist.id,
        artistName = artist.name,
        genre = genre.id,
        year = random:integer(2012, 2026),
        tracks = {},
    }
    album.cover = Catalog.newCover(album.id, random, genre)
    album.caption = string.format('%s · %d', artist.name, album.year)
    for number = 1, random:integer(6, 11) do
        local track = {
            id = 'track-' .. (#Catalog.tracks + 1),
            type = 'track',
            name = random:pick(Catalog.titleWords) .. ' ' .. random:pick(Catalog.titleNouns),
            artist = artist.id,
            artistName = artist.name,
            album = album.id,
            albumName = album.name,
            genre = genre.id,
            cover = album.cover,
            number = number,
            seed = #Catalog.tracks + 1,
        }
        track.length = Synth.plan(track.seed, Synth.styles[genre.style]).length
        track.durationText = Catalog.duration(track.length)
        track.caption = artist.name
        track.searchText = string.lower(track.name .. ' ' .. artist.name)
        Catalog.tracks[#Catalog.tracks + 1] = track
        album.tracks[number] = track
        artist.tracks[#artist.tracks + 1] = track
        Catalog.byId[track.id] = track
    end
    Catalog.albums[index] = album
    artist.albums[#artist.albums + 1] = album
    Catalog.byId[album.id] = album
end

-- The cover of an album or a mix: the name of a runtime image that `covers.lua` draws as an SVG document the first time it shows, from a style and colors the generator picks.
function Catalog.newCover(id, random, genre)
    local cover = '@cover-' .. id
    Catalog.coverSpecs = Catalog.coverSpecs or {}
    Catalog.coverSpecs[cover] = {
        style = random:integer(1, 6),
        background = genre.colors[1],
        second = genre.colors[2],
        accent = random:pick(Catalog.palette),
        turn = random:integer(0, 3) * 90,
        seed = random:integer(1, 100000),
    }
    return cover
end

function Catalog.find(id)
    return Catalog.byId[id]
end

-- Likes a song or takes the like back, keeps the liked songs newest first, and emits "playerChanged" so every view of the song follows.
function Catalog.toggleLike(track)
    track.liked = not track.liked
    if track.liked then
        table.insert(Catalog.liked, 1, track)
    else
        for index, other in ipairs(Catalog.liked) do
            if other == track then
                table.remove(Catalog.liked, index)
                break
            end
        end
    end
    events.emit('playerChanged')
    events.emit('notice', track.liked and 'Added to your liked songs.' or 'Removed from your liked songs.')
    return track.liked
end

function Catalog.thousands(number)
    local text = tostring(number):reverse():gsub('(%d%d%d)', '%1,'):reverse():gsub('^,', '')
    return text
end

Catalog.build()

return Catalog
