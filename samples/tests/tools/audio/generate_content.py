"""Synthesizes every sound of the audio tests: interface clicks, a chop, footsteps, a hit, a swing, a wooden pickup, an explosion, a fire loop, a hum, a plucked loop and two short music loops.

Run it from anywhere with `uv run --with numpy --with soundfile python3 samples/tests/tools/audio/generate_content.py`. The same run always writes the same files, as WAV, OGG Vorbis and MP3, the formats the tests read.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import soundfile

AUDIO = Path(__file__).resolve().parents[2] / "content" / "audio"


def seconds(duration: float, rate: int) -> np.ndarray:
    return np.arange(int(round(duration * rate))) / rate


def decay(t: np.ndarray, time: float) -> np.ndarray:
    return np.exp(-t / time)


def noise(count: int, seed: int) -> np.ndarray:
    return np.random.default_rng(seed).standard_normal(count)


def shape_spectrum(signal: np.ndarray, rate: int, gain) -> np.ndarray:
    """Filters a whole signal at once by a gain that `gain(frequencies)` gives for every frequency."""
    spectrum = np.fft.rfft(signal)
    frequencies = np.fft.rfftfreq(len(signal), 1 / rate)
    return np.fft.irfft(spectrum * gain(frequencies), len(signal))


def band(low: float, high: float, slope: float = 2.0):
    """Returns the gain of a smooth band pass, which a high or low edge of 0 or infinity leaves open."""

    def gain(frequencies: np.ndarray) -> np.ndarray:
        safe = np.maximum(frequencies, 1.0)
        rise = 1.0 if low <= 0 else 1 / (1 + (low / safe) ** (2 * slope))
        fall = 1.0 if high == np.inf else 1 / (1 + (safe / high) ** (2 * slope))
        return np.sqrt(rise * fall)

    return gain


def sweep_filter(signal: np.ndarray, rate: int, gain_at, frame: int = 1024) -> np.ndarray:
    """Filters a signal by a gain that changes over time: `gain_at(time, frequencies)` gives it for the middle of each frame, and the frames overlap by half under square-root Hann windows."""
    window = np.sqrt(np.hanning(frame + 1)[:frame])
    hop = frame // 2
    padded = np.concatenate([np.zeros(frame), signal, np.zeros(frame)])
    output = np.zeros_like(padded)
    frequencies = np.fft.rfftfreq(frame, 1 / rate)
    for start in range(0, len(padded) - frame, hop):
        piece = np.fft.rfft(padded[start : start + frame] * window)
        time = (start + hop - frame) / rate
        output[start : start + frame] += np.fft.irfft(piece * gain_at(time, frequencies), frame) * window
    return output[frame : frame + len(signal)]


def modes(t: np.ndarray, partials: list[tuple[float, float, float]]) -> np.ndarray:
    """Sums damped sines, each given as a frequency, an amplitude and a decay time, the way struck wood or metal rings."""
    total = np.zeros_like(t)
    for frequency, amplitude, time in partials:
        total += amplitude * np.sin(2 * np.pi * frequency * t) * decay(t, time)
    return total


def room(signal: np.ndarray, rate: int, length: float, mix: float, seed: int) -> np.ndarray:
    """Adds the tail of a small room, a convolution with decaying noise."""
    t = seconds(length, rate)
    response = noise(len(t), seed) * decay(t, length / 6)
    response = shape_spectrum(response, rate, band(0, 5000))
    response /= np.sqrt(np.sum(response**2))
    size = 1 << int(np.ceil(np.log2(len(signal) + len(response))))
    wet = np.fft.irfft(np.fft.rfft(signal, size) * np.fft.rfft(response, size), size)[: len(signal) + len(response) - 1]
    dry = np.concatenate([signal, np.zeros(len(response) - 1)])
    return dry + mix * wet


def fade_edges(signal: np.ndarray, rate: int, fade_in: float = 0.001, fade_out: float = 0.02) -> np.ndarray:
    result = signal.copy()
    rise, fall = max(1, int(fade_in * rate)), max(1, int(fade_out * rate))
    result[:rise] *= np.linspace(0, 1, rise)
    result[-fall:] *= np.linspace(1, 0, fall)
    return result


def normalize(signal: np.ndarray, peak: float) -> np.ndarray:
    return signal * (peak / np.max(np.abs(signal)))


def fit(signal: np.ndarray, rate: int, duration: float) -> np.ndarray:
    count = int(round(duration * rate))
    if len(signal) >= count:
        return signal[:count]
    return np.concatenate([signal, np.zeros(count - len(signal))])


def write(path: str, channels: list[np.ndarray] | np.ndarray, rate: int, kind: str, subtype: str) -> None:
    data = np.stack(channels, axis=1) if isinstance(channels, list) else channels
    target = AUDIO / path
    target.parent.mkdir(parents=True, exist_ok=True)
    soundfile.write(target, np.clip(data, -1, 1), rate, format=kind, subtype=subtype)


def ogg(path: str, channels, rate: int) -> None:
    write(path, channels, rate, "OGG", "VORBIS")


def click() -> None:
    rate = 44100
    t = seconds(0.1, rate)
    tick = 0.7 * np.sin(2 * np.pi * 3100 * t) * decay(t, 0.005) + 0.35 * np.sin(2 * np.pi * 950 * t) * decay(t, 0.012)
    snap = shape_spectrum(noise(len(t), 1), rate, band(2000, 9000)) * decay(t, 0.0015)
    ogg("ui/click.ogg", normalize(fade_edges(tick + 0.6 * snap, rate), 0.6), rate)


def confirm() -> None:
    rate = 44100
    t = seconds(0.32, rate)
    chime = np.zeros_like(t)
    for start, frequency in ((0.0, 880.0), (0.075, 1318.5)):
        local = np.maximum(t - start, 0)
        active = t >= start
        index = 1.6 * decay(local, 0.05)
        tone = np.sin(2 * np.pi * frequency * local + index * np.sin(2 * np.pi * frequency * 2 * local))
        chime += active * tone * decay(local, 0.11)
    ogg("ui/confirm.ogg", normalize(fade_edges(chime, rate), 0.6), rate)


def chop() -> None:
    rate = 48000
    channels = []
    for side in range(2):
        t = seconds(0.26, rate)
        body = modes(t, [(182, 0.9, 0.05), (406, 0.7, 0.04), (771, 0.5, 0.03), (1237, 0.35, 0.02), (1904, 0.2, 0.012)])
        thump = 0.8 * np.sin(2 * np.pi * 92 * t) * decay(t, 0.035)
        bite = shape_spectrum(noise(len(t), 10 + side), rate, band(1800, 6500)) * decay(t, 0.006)
        splinter = shape_spectrum(noise(len(t), 20 + side), rate, band(700, 3000)) * decay(np.maximum(t - 0.012, 0), 0.03) * (t > 0.012)
        channels.append(fade_edges(body + thump + 1.4 * bite + 0.25 * splinter, rate))
    peak = max(np.max(np.abs(channel)) for channel in channels)
    ogg("effects/chop.ogg", [0.9 * channel / peak for channel in channels], rate)


def hit() -> None:
    rate = 44100
    channels = []
    for side in range(2):
        t = seconds(0.43, rate)
        frequency = 55 + 125 * decay(t, 0.05)
        phase = 2 * np.pi * np.cumsum(frequency) / rate
        punch = np.sin(phase) * decay(t, 0.11)
        smack = shape_spectrum(noise(len(t), 30 + side), rate, band(200, 1600)) * decay(t, 0.025)
        snap = shape_spectrum(noise(len(t), 40 + side), rate, band(3000, 10000)) * decay(t, 0.002)
        channels.append(fade_edges(punch + 0.7 * smack + 0.4 * snap, rate, fade_out=0.05))
    peak = max(np.max(np.abs(channel)) for channel in channels)
    ogg("effects/hit_1.ogg", [0.9 * channel / peak for channel in channels], rate)


def crunch(t: np.ndarray, rate: int, start: float, length: float, seed: int, low: float, high: float) -> np.ndarray:
    """A burst of gravel: grains of filtered noise that thin out over `length` seconds."""
    rng = np.random.default_rng(seed)
    local = np.maximum(t - start, 0)
    grains = np.zeros_like(t)
    for _ in range(int(length * 900)):
        at = start + rng.exponential(length / 3)
        index = int(at * rate)
        if index < len(t):
            grains[index] += rng.standard_normal() * np.exp(-(at - start) / length)
    texture = shape_spectrum(grains, rate, band(low, high))
    texture += 0.3 * shape_spectrum(noise(len(t), seed + 1), rate, band(low, high)) * decay(local, length / 2) * (t >= start)
    return texture


def footsteps() -> None:
    rate = 44100
    variations = [(0.105, 1.0, 900), (0.095, 0.85, 1100), (0.12, 0.95, 800), (0.1, 0.8, 1000)]
    for number, (toe, weight, low) in enumerate(variations, start=1):
        channels = []
        for side in range(2):
            t = seconds(0.4, rate)
            seed = number * 10 + side
            heel = crunch(t, rate, 0.0, 0.06, seed, low * 0.35, 3200)
            ball = crunch(t, rate, toe, 0.05, seed + 5, low * 0.45, 2800)
            thud = weight * np.sin(2 * np.pi * (78 + number * 4) * t) * decay(t, 0.03)
            step = heel + 0.6 * ball
            step = 0.8 * step / np.max(np.abs(step)) + 0.5 * thud
            channels.append(fit(fade_edges(room(step, rate, 0.35, 0.18, seed + 7), rate, fade_out=0.08), rate, 0.65))
        peak = max(np.max(np.abs(channel)) for channel in channels)
        ogg(f"effects/footstep_{number}.ogg", [0.85 * channel / peak for channel in channels], rate)


def swing() -> None:
    rate = 44100
    duration = 0.2
    channels = []
    for side in range(2):
        t = seconds(duration, rate)
        air = noise(len(t), 50 + side)

        def gain_at(time: float, frequencies: np.ndarray) -> np.ndarray:
            progress = min(max(time / duration, 0), 1)
            center = 380 + 1500 * np.sin(np.pi * progress) ** 2
            return np.exp(-0.5 * (np.log2(np.maximum(frequencies, 1) / center) / 0.55) ** 2)

        whoosh = sweep_filter(air, rate, gain_at, 512)
        envelope = np.sin(np.pi * np.clip(t / duration, 0, 1)) ** 3
        pan = 0.35 + 0.65 * (t / duration if side == 1 else 1 - t / duration)
        channels.append(fade_edges(whoosh * envelope * pan, rate, fade_out=0.01))
    peak = max(np.max(np.abs(channel)) for channel in channels)
    write("effects/swing_1.wav", [0.85 * channel / peak for channel in channels], rate, "WAV", "PCM_24")


def wood_pickup() -> None:
    rate = 48000
    channels = []
    for side in range(2):
        t = seconds(0.54, rate)
        first = modes(t, [(320, 0.8, 0.045), (690, 0.6, 0.035), (1310, 0.35, 0.02), (2140, 0.2, 0.01)])
        later = np.maximum(t - 0.13, 0)
        second = (t >= 0.13) * modes(later, [(250, 0.7, 0.05), (585, 0.5, 0.04), (1120, 0.3, 0.025)])
        rustle_envelope = np.clip(t / 0.03, 0, 1) * decay(t, 0.16)
        rustle = shape_spectrum(noise(len(t), 60 + side), rate, band(500, 4500)) * rustle_envelope
        knock = shape_spectrum(noise(len(t), 70 + side), rate, band(1500, 6000)) * (decay(t, 0.004) + (t >= 0.13) * decay(later, 0.004))
        channels.append(fade_edges(room(first + 0.8 * second + 0.12 * rustle + 0.6 * knock, rate, 0.3, 0.12, 80 + side), rate, fade_out=0.06))
    channels = [fit(channel, rate, 0.54) for channel in channels]
    peak = max(np.max(np.abs(channel)) for channel in channels)
    ogg("effects/wood_pickup.ogg", [0.9 * channel / peak for channel in channels], rate)


def explosion() -> None:
    rate = 48000
    duration = 1.6
    channels = []
    for side in range(2):
        t = seconds(duration, rate)
        roar = noise(len(t), 90 + side)

        def gain_at(time: float, frequencies: np.ndarray) -> np.ndarray:
            cutoff = 250 + 7000 * np.exp(-max(time, 0) / 0.18)
            return 1 / np.sqrt(1 + (np.maximum(frequencies, 1) / cutoff) ** 4)

        body = sweep_filter(roar, rate, gain_at) * (np.clip(t / 0.004, 0, 1) * decay(t, 0.42))
        frequency = 32 + 48 * decay(t, 0.15)
        boom = np.sin(2 * np.pi * np.cumsum(frequency) / rate) * decay(t, 0.55)
        rumble = shape_spectrum(noise(len(t), 105 + side), rate, band(25, 160)) * np.clip(t / 0.01, 0, 1) * decay(t, 0.6)
        crack = shape_spectrum(noise(len(t), 100 + side), rate, band(1500, 12000)) * decay(t, 0.012)
        debris = crunch(t, rate, 0.25, 0.5, 110 + side, 1200, 6000)
        mix = body / np.max(np.abs(body)) + 0.9 * rumble / np.max(np.abs(rumble)) + 0.45 * boom + 0.5 * crack + 0.08 * debris
        channels.append(fade_edges(mix, rate, fade_out=0.2))
    peak = max(np.max(np.abs(channel)) for channel in channels)
    ogg("effects/explosion.ogg", [np.tanh(1.4 * channel / peak) * 0.95 for channel in channels], rate)


def loop_crossfade(signal: np.ndarray, rate: int, duration: float, overlap: float) -> np.ndarray:
    """Returns the first `duration` seconds of a longer signal with its overflow faded into its start, so the end joins the start without a seam."""
    count, blend = int(duration * rate), int(overlap * rate)
    looped = signal[:count].copy()
    ramp = np.linspace(0, np.pi / 2, blend)
    looped[:blend] = looped[:blend] * np.sin(ramp) + signal[count : count + blend] * np.cos(ramp)
    return looped


def fire_loop() -> None:
    rate = 44100
    duration, overlap = 4.0, 0.5
    t = seconds(duration + overlap, rate)
    rng = np.random.default_rng(120)

    rumble = shape_spectrum(noise(len(t), 121), rate, band(40, 380))
    flutter = 0.6 + 0.4 * shape_spectrum(noise(len(t), 122), rate, band(0, 3))
    flutter /= np.max(np.abs(flutter))
    hiss = shape_spectrum(noise(len(t), 123), rate, band(2500, 9000))
    bed = rumble / np.max(np.abs(rumble)) * flutter + 0.08 * hiss / np.max(np.abs(hiss))

    crackles = np.zeros_like(t)
    time = 0.0
    while time < duration + overlap:
        time += rng.exponential(1 / 14)
        start = int(time * rate)
        size = int(rng.uniform(0.002, 0.012) * rate)
        if start + size >= len(t):
            break
        burst = rng.standard_normal(size) * np.exp(-np.arange(size) / (size / 4))
        crackles[start : start + size] += burst * rng.pareto(2.5) * 0.4
    crackles = shape_spectrum(crackles, rate, band(900, 7000))
    fire = loop_crossfade(0.55 * bed + crackles, rate, duration, overlap)
    ogg("ambient/fire_loop.ogg", np.tanh(normalize(fire, 1.2)) * 0.8, rate)


def hum_loop() -> None:
    rate = 22050
    t = seconds(1.0, rate)
    harmonics = [(110, 1.0), (220, 0.55), (330, 0.32), (440, 0.18), (550, 0.1), (770, 0.05)]
    tone = sum(amplitude * np.sin(2 * np.pi * frequency * t + index) for index, (frequency, amplitude) in enumerate(harmonics))
    wobble = 1 + 0.18 * np.sin(2 * np.pi * 4 * t)
    write("generated/hum_loop.wav", normalize(tone * wobble, 0.7), rate, "WAV", "PCM_16")


def midi(note: int) -> float:
    return 440.0 * 2 ** ((note - 69) / 12)


def pluck(t: np.ndarray, frequency: float, brightness: float = 1.0) -> np.ndarray:
    """A plucked string from harmonics whose upper partials fade first."""
    total = np.zeros_like(t)
    for harmonic in range(1, 13):
        partial = frequency * harmonic
        if partial > 9000:
            break
        amplitude = brightness**harmonic / harmonic
        total += amplitude * np.sin(2 * np.pi * partial * t * (1 + 0.0004 * harmonic)) * decay(t, 0.9 / harmonic**0.8)
    return total * np.clip(t / 0.002, 0, 1)


def pad(t: np.ndarray, frequency: float, length: float) -> np.ndarray:
    """A soft chord voice from slightly detuned harmonics with slow edges."""
    total = np.zeros_like(t)
    for detune in (-0.004, 0.0, 0.005):
        for harmonic in range(1, 8):
            total += np.sin(2 * np.pi * frequency * harmonic * (1 + detune) * t + harmonic * detune * 300) / harmonic**1.6
    envelope = np.clip(t / 0.6, 0, 1) * np.clip((length - t) / 0.8, 0, 1)
    return total * envelope


def bell(t: np.ndarray, frequency: float) -> np.ndarray:
    index = 2.2 * decay(t, 0.25)
    return np.sin(2 * np.pi * frequency * t + index * np.sin(2 * np.pi * frequency * 3.5 * t)) * decay(t, 0.7) * np.clip(t / 0.003, 0, 1)


def bass(t: np.ndarray, frequency: float, length: float) -> np.ndarray:
    tone = np.sin(2 * np.pi * frequency * t) + 0.35 * np.sin(2 * np.pi * frequency * 2 * t) + 0.12 * np.sin(2 * np.pi * frequency * 3 * t)
    return tone * decay(t, 0.6) * np.clip(t / 0.005, 0, 1) * np.clip((length - t) / 0.05, 0, 1)


def kick(t: np.ndarray) -> np.ndarray:
    frequency = 45 + 90 * decay(t, 0.04)
    return np.sin(2 * np.pi * np.cumsum(frequency) / 44100) * decay(t, 0.18)


def hat(t: np.ndarray, seed: int) -> np.ndarray:
    return shape_spectrum(noise(len(t), seed), 44100, band(6000, 14000)) * decay(t, 0.03)


class Arrangement:
    """Collects the notes of a loop and renders them three times over, keeping the middle pass, so tails that cross the end of the loop sound at its start."""

    rate = 44100

    def __init__(self, bars: int, tempo: float) -> None:
        self.beat = 60 / tempo
        self.length = bars * 4 * self.beat
        self.voices: list[tuple[float, float, float, np.ndarray]] = []

    def add(self, beat: float, pan: float, gain: float, sound: np.ndarray) -> None:
        self.voices.append((beat * self.beat, pan, gain, sound))

    def render(self) -> list[np.ndarray]:
        count = int(round(self.length * self.rate))
        left, right = np.zeros(count * 3 + self.rate * 4), np.zeros(count * 3 + self.rate * 4)
        for start, pan, gain, sound in self.voices:
            for repeat in range(3):
                index = int(round((start + repeat * self.length) * self.rate))
                left[index : index + len(sound)] += sound * gain * np.cos((pan + 1) * np.pi / 4)
                right[index : index + len(sound)] += sound * gain * np.sin((pan + 1) * np.pi / 4)
        mixed = []
        for side, channel in enumerate((left, right)):
            wet = room(channel, self.rate, 1.8, 0.35, 200 + side)[: len(channel)]
            mixed.append(wet[count : count * 2])
        peak = max(np.max(np.abs(channel)) for channel in mixed)
        return [np.tanh(1.3 * channel / peak) * 0.88 for channel in mixed]


def sound(length: float) -> np.ndarray:
    return seconds(length, Arrangement.rate)


def calm_loop() -> None:
    song = Arrangement(bars=8, tempo=80)
    chords = [(57, [57, 60, 64]), (53, [53, 57, 60]), (48, [55, 60, 64]), (55, [55, 59, 62]), (57, [57, 60, 64]), (53, [53, 57, 60]), (55, [55, 59, 62]), (52, [52, 56, 59])]
    melody = [(0, 76, 2), (2, 74, 1), (3, 72, 1), (4, 72, 3), (8, 71, 2), (10, 72, 1), (11, 74, 1), (12, 71, 4), (16, 76, 2), (18, 79, 2), (20, 77, 2), (22, 76, 2), (24, 74, 3), (27, 72, 1), (28, 71, 4)]
    for bar, (root, chord) in enumerate(chords):
        start = bar * 4
        for index, note in enumerate(chord):
            song.add(start, -0.4 + 0.4 * index, 0.12, pad(sound(4 * song.beat + 0.8), midi(note), 4 * song.beat))
        song.add(start, 0.0, 0.5, bass(sound(2 * song.beat), midi(root - 12), 2 * song.beat))
        song.add(start + 2, 0.0, 0.4, bass(sound(2 * song.beat), midi(root - 12), 2 * song.beat))
        pattern = [chord[0], chord[1], chord[2], chord[1] + 12, chord[2], chord[1], chord[0] + 12, chord[2]]
        for step, note in enumerate(pattern):
            song.add(start + step * 0.5, 0.3 if step % 2 else -0.3, 0.22, pluck(sound(1.6), midi(note + 12), 0.6))
    for beat, note, length in melody:
        song.add(beat, 0.1, 0.28, bell(sound(length * song.beat + 1.0), midi(note)))
    write("music/calm_loop.mp3", song.render(), Arrangement.rate, "MP3", "MPEG_LAYER_III")


def lively_loop() -> None:
    song = Arrangement(bars=12, tempo=120)
    progression = [(48, [60, 64, 67]), (55, [59, 62, 67]), (57, [60, 64, 69]), (53, [60, 65, 69])]
    phrase = [(0, 72), (0.5, 74), (1, 76), (2, 79), (3, 76), (4, 74), (4.5, 72), (5, 74), (6, 76), (7, 74), (8, 72), (8.5, 74), (9, 76), (10, 81), (11, 79), (12, 77), (13, 76), (14, 74), (15, 72)]
    for bar in range(12):
        root, chord = progression[bar % 4]
        start = bar * 4
        for eighth in range(8):
            note = root - 12 if eighth % 4 != 3 else root - 5
            song.add(start + eighth * 0.5, 0.0, 0.42, bass(sound(0.5 * song.beat), midi(note), 0.45 * song.beat))
        for beat in (0.5, 1.5, 2.5, 3.5):
            for index, note in enumerate(chord):
                song.add(start + beat, -0.3 + 0.3 * index, 0.1, pluck(sound(0.8), midi(note), 0.5))
        for beat in (0, 2):
            song.add(start + beat, 0.0, 0.7, kick(sound(0.4)))
        for beat in range(8):
            song.add(start + beat * 0.5 + 0.5, 0.25, 0.12 if beat % 2 else 0.06, hat(sound(0.12), 300 + bar * 8 + beat))
    for repeat in range(3):
        for beat, note in phrase:
            song.add(repeat * 16 + beat, -0.15, 0.3, pluck(sound(1.2), midi(note), 0.75))
    write("music/lively_loop.mp3", song.render(), Arrangement.rate, "MP3", "MPEG_LAYER_III")


def pluck_loop() -> None:
    rate = 22050
    beat = 0.5
    pattern = [57, 60, 64, 69, 67, 64, 60, 64]
    length = len(pattern) * beat / 2
    count = int(length * rate)
    output = np.zeros(count * 3 + rate * 2)
    for repeat in range(3):
        for step, note in enumerate(pattern):
            t = seconds(1.5, rate)
            start = int((repeat * length + step * beat / 2) * rate)
            output[start : start + len(t)] += pluck(t, midi(note), 0.7)
    write("generated/pluck_loop.wav", normalize(output[count : count * 2], 0.7), rate, "WAV", "PCM_16")


def main() -> None:
    click()
    confirm()
    chop()
    hit()
    footsteps()
    swing()
    wood_pickup()
    explosion()
    fire_loop()
    hum_loop()
    pluck_loop()
    calm_loop()
    lively_loop()


if __name__ == "__main__":
    main()
