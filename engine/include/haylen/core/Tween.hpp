#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "haylen/core/Connection.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"

namespace haylen::core {

class PropertyTween;
class Timeline;
class TweenManager;

// Anything that plays over time: a PropertyTween that animates values or a Timeline that plays other tweens. A tween has a delay, repeats a number of times or forever with restart, yoyo or incremental loops and an optional pause between loops, and can play, pause, reverse, seek and complete. Times exclude the delay. The manager plays root tweens, and a timeline plays the tweens placed in it.
class Tween : public Connection::Link, public std::enable_shared_from_this<Tween> {
  public:
    // Restart plays every loop from the start, Yoyo plays every other loop backwards and Incremental continues every loop from where the previous one ended.
    enum class LoopMode : std::uint8_t {
        Restart,
        Yoyo,
        Incremental,
    };

    // Start runs when the tween starts playing forward after its delay, update after every render with the progress, loop with the number of the loop that begins, step with the index of a timeline step that ends, complete when the tween reaches its end in the direction it plays and kill once when it leaves.
    struct Callbacks {
        std::function<void()> start;
        std::function<void(float progress)> update;
        std::function<void(int loop)> loop;
        std::function<void(int step)> step;
        std::function<void()> complete;
        std::function<void()> kill;
    };

    ~Tween() override = default;

    Tween(const Tween&) = delete;
    Tween& operator=(const Tween&) = delete;

    void setDelay(float seconds);
    [[nodiscard]] float getDelay() const noexcept {
        return delay;
    }

    // A negative count repeats forever.
    void setRepeatCount(int count) noexcept;
    [[nodiscard]] int getRepeatCount() const noexcept {
        return repeatCount;
    }
    void setLoopMode(LoopMode value) noexcept {
        loopMode = value;
    }
    [[nodiscard]] LoopMode getLoopMode() const noexcept {
        return loopMode;
    }
    void setRepeatDelay(float seconds);
    [[nodiscard]] float getRepeatDelay() const noexcept {
        return repeatDelay;
    }

    void setTimeScale(float value);
    [[nodiscard]] float getTimeScale() const noexcept {
        return timeScale;
    }

    // Root tweens run by their process mode and on scaled or unscaled time, and fixed-step tweens advance with the fixed updates of physics.
    void setProcessMode(ProcessMode value) noexcept {
        processMode = value;
    }
    [[nodiscard]] ProcessMode getProcessMode() const noexcept {
        return processMode;
    }

    // Gives an inheriting tween the mode of its parent, such as the scene that owns it. The manager asks on every update, so the tween follows the parent when its mode changes. Without one, Inherit counts as Pausable.
    void setParentMode(std::function<ProcessMode()> value) {
        parentMode = std::move(value);
    }

    // Returns the mode the tween runs by: its own, or the one of its parent while it inherits.
    [[nodiscard]] ProcessMode resolveProcessMode() const;
    void setUnscaled(bool value) noexcept {
        unscaled = value;
    }
    [[nodiscard]] bool isUnscaled() const noexcept {
        return unscaled;
    }
    void setFixedStep(bool value) noexcept {
        fixedStep = value;
    }
    [[nodiscard]] bool isFixedStep() const noexcept {
        return fixedStep;
    }

    // A root tween that auto-kills leaves the manager when it completes. Turn it off to replay, reverse or seek a finished tween.
    void setAutoKill(bool value) noexcept {
        autoKill = value;
    }
    [[nodiscard]] bool isAutoKill() const noexcept {
        return autoKill;
    }

    // Groups the tween with others for TweenManager::killTag, pauseTag and setTimeScale. It is fixed once the manager plays the tween.
    void setTag(std::string value);
    [[nodiscard]] const std::string& getTag() const noexcept {
        return tag;
    }

    void setCallbacks(Callbacks value) {
        callbacks = std::move(value);
    }
    [[nodiscard]] Callbacks& getCallbacks() noexcept {
        return callbacks;
    }

    // Plays forward from the current time.
    void play();
    void pause() noexcept;

    // Continues in the current direction.
    void resume() noexcept;

    // Goes back to the start, waits the delay again and plays forward.
    void restart();

    // Plays backwards from the current time and completes at the start.
    void reverse();

    // Jumps to a time without running callbacks. Seeking a timeline renders every tween it passes, and seeking an infinite tween moves within the current loop.
    void seek(float seconds);
    void setProgress(float value);

    // Jumps to the end in the direction the tween plays, with or without the callbacks on the way. It has no effect on a tween that repeats forever.
    void complete(bool withCallbacks = true);

    // Stops the tween for good. It runs the kill callback, emits finished with false, and a tween in a timeline leaves it. The automatic kill of a tween that just completed emits nothing more.
    void kill();

    [[nodiscard]] bool isPlaying() const noexcept {
        return !killed && !paused && !completed;
    }
    [[nodiscard]] bool isPaused() const noexcept {
        return paused;
    }
    [[nodiscard]] bool isReversed() const noexcept {
        return reversed;
    }
    [[nodiscard]] bool isCompleted() const noexcept {
        return completed;
    }
    [[nodiscard]] bool isAlive() const noexcept {
        return !killed;
    }

    [[nodiscard]] float getTime() const noexcept {
        return time;
    }

    // Returns how far the tween is through all its loops, or through the current loop when it repeats forever.
    [[nodiscard]] float getProgress() const noexcept;

    // Returns the length of one loop, which a speed-based tween only knows once it starts.
    [[nodiscard]] virtual float getDuration() const noexcept = 0;

    // Returns the length of all loops with the pauses between them, which is infinite for a tween that repeats forever.
    [[nodiscard]] float getTotalDuration() const noexcept;

    [[nodiscard]] Connection getConnection() {
        return Connection(std::weak_ptr<Connection::Link>(shared_from_this()));
    }

    // Kills the tween, which is how a connection scope ends it. Blocking pauses it.
    void disconnect() override {
        kill();
    }
    [[nodiscard]] bool isConnected() const noexcept override {
        return !killed;
    }
    void setBlocked(bool value) override {
        paused = value;
    }
    [[nodiscard]] bool isBlocked() const noexcept override {
        return paused;
    }

    // Emits true every time the tween completes and false when it is killed before completing, which is what waiting on a tween resumes on.
    Signal<bool> finished;

  protected:
    Tween() = default;

    // Resolves what the tween needs before its first render, such as the start values of its properties.
    virtual void prepare();

    // Renders the tween at a time inside one loop. Previous is where the loop was before, and it lies outside the loop when the loop was just entered, so zero-length callbacks on its edges run. Loops counts the completed loops of an incremental tween.
    virtual void renderLoop(float loopTime, float previous, int loops, bool silent) = 0;

    // Returns the progress to report to the update callback.
    [[nodiscard]] virtual float getRenderedProgress() const noexcept;

    // Kills the tweens a timeline holds.
    virtual void killChildren();

    // Drops the tween without running any callback and lets go of everything it holds, which is what shutdown needs before the Lua state closes.
    virtual void discard() noexcept;

    // Kills the tween when it animates the target, or the tweens of a timeline that do. Returns whether the tween itself was killed.
    virtual bool killTarget(const void* target);

    // Gives up the fields that a newer tween now animates on the same targets, killing whatever is left with nothing to animate.
    virtual void yieldTo(const PropertyTween& newer);

    [[nodiscard]] bool isInTimeline() const noexcept {
        return parent != nullptr;
    }
    [[nodiscard]] bool hasRendered() const noexcept {
        return rendered;
    }

  private:
    friend class Timeline;
    friend class TweenManager;

    struct Position {
        int loop = 0;
        float time = 0.0F;
    };

    static debug::ObjectCounter& counter;

    [[nodiscard]] Position locate(float totalTime) const noexcept;
    [[nodiscard]] float getLoopStart(int loop) const noexcept;
    [[nodiscard]] float getLoopEnd(int loop) const noexcept;

    // Moves the playhead to a time between 0 and the total duration and renders it. Timelines call it for the tweens they hold.
    void render(float totalTime, bool silent);

    // Advances a root tween by the elapsed time, spending the delay first.
    void advance(float seconds);

    void end(bool justCompleted);

    Callbacks callbacks;
    std::function<ProcessMode()> parentMode;
    std::string tag;
    TweenManager* manager = nullptr;
    Timeline* parent = nullptr;
    std::size_t group = 0;
    float delay = 0.0F;
    float delayLeft = 0.0F;
    float repeatDelay = 0.0F;
    float timeScale = 1.0F;
    float time = 0.0F;
    int repeatCount = 0;
    LoopMode loopMode = LoopMode::Restart;
    ProcessMode processMode = ProcessMode::Inherit;
    bool unscaled = false;
    bool fixedStep = false;
    bool autoKill = true;
    bool paused = false;
    bool reversed = false;
    bool started = false;
    bool completed = false;
    bool killed = false;
    bool rendered = false;

    // Set until the playhead leaves the start moving forward, and again when it goes back to the start, so callbacks at time zero run every time the tween plays from its start.
    bool atStart = true;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::core
