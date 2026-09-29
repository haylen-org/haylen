#include "haylen/core/SceneManager.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SceneTransition.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::core {

SceneManager::Transition SceneManager::Transition::fade(float seconds, math::Color color) {
    return {.duration = seconds, .effect = std::make_shared<graphics2d::SceneTransition>(graphics2d::SceneTransition::Options{.kind = graphics2d::SceneTransition::Kind::Fade, .color = color})};
}

SceneManager::SceneManager(Engine& owner) : engine(owner) {}

SceneManager::~SceneManager() = default;

void SceneManager::push(std::shared_ptr<Scene> scene) {
    push(std::move(scene), Options{});
}

void SceneManager::push(std::shared_ptr<Scene> scene, Options options) {
    if (!scene) {
        throw std::invalid_argument("Cannot push an empty scene.");
    }
    request({.operation = Operation::Push, .scene = std::move(scene), .options = std::move(options)});
}

void SceneManager::replace(std::shared_ptr<Scene> scene) {
    replace(std::move(scene), Options{});
}

void SceneManager::replace(std::shared_ptr<Scene> scene, Options options) {
    if (!scene) {
        throw std::invalid_argument("Cannot replace with an empty scene.");
    }
    request({.operation = Operation::Replace, .scene = std::move(scene), .options = std::move(options)});
}

void SceneManager::pop() {
    pop(Transition{});
}

void SceneManager::pop(Transition transition, Completion completion) {
    request({.operation = Operation::Pop, .options = {.transition = std::move(transition), .completion = std::move(completion)}});
}

void SceneManager::popTo(std::size_t level) {
    popTo(level, Transition{});
}

void SceneManager::popTo(std::size_t level, Transition transition, Completion completion) {
    request({.operation = Operation::PopTo, .level = level, .options = {.transition = std::move(transition), .completion = std::move(completion)}});
}

void SceneManager::popToRoot() {
    popTo(1, Transition{});
}

void SceneManager::popToRoot(Transition transition, Completion completion) {
    popTo(1, std::move(transition), std::move(completion));
}

void SceneManager::request(Change change) {
    const Options& options = change.options;
    if (options.transition.duration > 0.0F && !options.transition.effect) {
        throw std::invalid_argument("A scene transition with a duration needs an effect.");
    }
    if (options.loadingDelay < 0.0F || options.minimumLoadingTime < 0.0F || options.loadingFadeOut < 0.0F) {
        throw std::invalid_argument("The loading delay, the minimum loading time and the loading fade-out cannot be negative.");
    }

    auto shared = std::make_shared<Change>(std::move(change));
    if (pending) {
        queue.push_back(std::move(shared));
        return;
    }
    pending = std::move(shared);
}

void SceneManager::preload(std::shared_ptr<Scene> scene, std::any params, Completion completion) {
    if (!scene) {
        throw std::invalid_argument("Cannot preload an empty scene.");
    }
    if (!needsLoad(scene.get())) {
        throw std::invalid_argument("The scene is already loaded or on the stack.");
    }
    preloaded.push_back(scene);
    if (completion) {
        watchers.emplace_back(scene, std::move(completion));
    }
    startLoad(scene, std::move(params));
}

void SceneManager::cancelPreload(const Scene& scene) {
    const auto found = std::ranges::find_if(preloaded, [&scene](const std::shared_ptr<Scene>& candidate) { return candidate.get() == &scene; });
    if (found == preloaded.end()) {
        throw std::invalid_argument("The scene is not preloaded.");
    }
    const std::shared_ptr<Scene> cancelled = *found;
    preloaded.erase(found);
    unloadScene(cancelled);
    notifyWatchers(*cancelled, {.outcome = Outcome::Dropped});
}

void SceneManager::clear() {
    // Every scene leaves and every completion runs even when one of them fails, and the first failure is reported at the end.
    std::exception_ptr failure;
    // clang-format off
    const auto attempt = [&failure](const auto& step) {
        try {
            step();
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
    };
    // clang-format on

    // The pending change stays pending while the scenes leave, so changes that their hooks request queue behind it and are dropped with it.
    if (pending && pending->viewShown) {
        attempt([this] { hideLoadingView(*pending); });
    }
    while (!stack.empty()) {
        const std::shared_ptr<Scene> scene = stack.back();
        stack.pop_back();
        attempt([this, &scene] { retire(scene); });
    }
    for (const std::shared_ptr<Scene>& scene : std::exchange(leaving, {})) {
        attempt([this, &scene] { retire(scene); });
    }

    // The next scene of the pending change and the preloaded scenes unload without entering.
    std::vector<std::shared_ptr<Scene>> loaded = std::exchange(preloaded, {});
    if (pending && pending->scene && (pending->scene->state == Scene::State::Loading || pending->scene->state == Scene::State::Loaded)) {
        loaded.push_back(pending->scene);
    }
    for (const std::shared_ptr<Scene>& scene : loaded) {
        attempt([this, &scene] { unloadScene(scene); });
    }

    std::vector<std::shared_ptr<Change>> dropped(std::make_move_iterator(queue.begin()), std::make_move_iterator(queue.end()));
    queue.clear();
    if (pending) {
        dropped.insert(dropped.begin(), std::exchange(pending, {}));
    }
    endTransition();
    for (const auto& [scene, completion] : std::exchange(watchers, {})) {
        attempt([&completion] { completion({.outcome = Outcome::Dropped}); });
    }
    for (const std::shared_ptr<Change>& change : dropped) {
        if (change->options.completion) {
            attempt([&change] { change->options.completion({.outcome = Outcome::Dropped}); });
        }
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

Scene* SceneManager::getTop() const noexcept {
    return stack.empty() ? nullptr : stack.back().get();
}

std::shared_ptr<Scene> SceneManager::getTopShared() const noexcept {
    return stack.empty() ? nullptr : stack.back();
}

Scene& SceneManager::at(std::size_t index) const {
    if (index >= stack.size()) {
        throw std::out_of_range("The scene stack has no scene at that index.");
    }
    return *stack[index];
}

std::optional<std::size_t> SceneManager::find(const Scene& scene) const noexcept {
    const auto found = std::find_if(stack.begin(), stack.end(), [&scene](const std::shared_ptr<Scene>& candidate) { return candidate.get() == &scene; });
    if (found == stack.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - stack.begin());
}

ProcessMode SceneManager::getProcessMode(std::size_t index) const {
    for (std::size_t level = std::min(index + 1, stack.size()); level > 0; --level) {
        const ProcessMode mode = stack[level - 1]->getProcessMode();
        if (mode != ProcessMode::Inherit) {
            return mode;
        }
    }
    return ProcessMode::Pausable;
}

void SceneManager::notifyPauseChange(bool gamePaused) {
    // The hooks may change the stack, so the scenes to tell are found first and each one is told only while it is still on the stack.
    std::vector<std::pair<std::shared_ptr<Scene>, bool>> changed;
    for (std::size_t index = 0; index < stack.size(); ++index) {
        const ProcessMode mode = getProcessMode(index);
        const bool running = FrameClock::canProcess(mode, gamePaused);
        if (running != FrameClock::canProcess(mode, !gamePaused)) {
            changed.emplace_back(stack[index], running);
        }
    }

    for (const auto& [scene, running] : changed) {
        if (!find(*scene)) {
            continue;
        }
        if (running) {
            scene->unpaused(engine);
        } else {
            scene->paused(engine);
        }
    }
}

// A covered scene waits under the scenes on top of it, so only a top scene that is not covered runs.
bool SceneManager::canProcessTop(const FrameClock& clock) const {
    return !stack.empty() && stack.back()->state != Scene::State::Covered && clock.canProcess(getProcessMode(stack.size() - 1));
}

bool SceneManager::needsLoad(const Scene* scene) noexcept {
    return scene != nullptr && (scene->state == Scene::State::Created || scene->state == Scene::State::Unloaded);
}

void SceneManager::startLoad(const std::shared_ptr<Scene>& scene, std::any params) {
    auto load = std::make_shared<SceneLoad>(engine, std::move(params));
    scene->state = Scene::State::Loading;
    scene->loading = load;
    publish(LifecycleEvent::kSceneLoading, *scene);
    if (scene->loading != load) {
        return;
    }

    // A hook that throws fails the load like a failed deferral, and the load settles at once when the hook finished it.
    try {
        scene->load(engine, *load);
    } catch (const lua::Error& error) {
        load->fail(error);
    } catch (const std::exception& exception) {
        load->fail(lua::Error(exception.what()));
    }
    load->release();
    if (scene->loading == load) {
        settle(scene);
    }
}

void SceneManager::settle(const std::shared_ptr<Scene>& scene) {
    const std::shared_ptr<SceneLoad> load = scene->loading;
    if (scene->state != Scene::State::Loading || !load || !(load->isFinished() || load->getError())) {
        return;
    }

    if (load->isFinished()) {
        scene->state = Scene::State::Loaded;
        publish(LifecycleEvent::kSceneLoaded, *scene);
        notifyWatchers(*scene, {});
        return;
    }

    const lua::Error error = *load->getError();
    (void)engine.getEvents().emitWith(LifecycleEvent::kSceneLoadFailed, LoadFailure{.scene = scene.get(), .error = &error});
    std::erase(preloaded, scene);
    if (pending && pending->scene == scene && pending->phase != Phase::Start) {
        pending->failure = error;
    }
    unloadScene(scene);
    notifyWatchers(*scene, {.outcome = Outcome::Failed, .error = error});
}

// Settling runs the listeners of the load events, which may change which scenes load, so the loading scenes are taken first.
void SceneManager::settleLoads() {
    std::vector<std::shared_ptr<Scene>> loading = preloaded;
    if (pending && pending->scene && pending->phase != Phase::Start) {
        loading.push_back(pending->scene);
    }
    for (const std::shared_ptr<Scene>& scene : loading) {
        settle(scene);
    }
}

void SceneManager::notifyWatchers(const Scene& scene, const Result& result) {
    std::vector<Completion> matched;
    for (auto entry = watchers.begin(); entry != watchers.end();) {
        if (entry->first.get() != &scene) {
            ++entry;
            continue;
        }
        matched.push_back(std::move(entry->second));
        entry = watchers.erase(entry);
    }
    for (const Completion& completion : matched) {
        completion(result);
    }
}

void SceneManager::publish(std::string_view name, const Scene& scene) {
    (void)engine.getEvents().emitWith(name, scene);
}

// Cover and hold events carry the scene the change brings on top, and reveal events the scene on top once the change applied or restored the stack.
void SceneManager::publishPhase(std::string_view name, const Change& change) {
    const Scene* to = change.scene.get();
    if (change.phase == Phase::Reveal || (!change.scene && change.phase == Phase::Hold)) {
        to = getTop();
    } else if (!change.scene) {
        const std::size_t keep = getKeptLevel(change);
        to = keep > 0 ? stack[keep - 1].get() : nullptr;
    }
    (void)engine.getEvents().emitWith(name, Transfer{.from = change.from.get(), .to = to});
}

std::size_t SceneManager::getKeptLevel(const Change& change) const noexcept {
    switch (change.operation) {
    case Operation::Push:
        return stack.size();
    case Operation::Replace:
    case Operation::Pop:
        return stack.empty() ? 0 : stack.size() - 1;
    case Operation::PopTo:
        return std::min(change.level, stack.size());
    }
    return stack.size();
}

void SceneManager::update(const FrameClock& clock) {
    const auto delta = static_cast<float>(clock.getUnscaledDelta());
    settleLoads();
    advance(delta);

    if (isLoadingViewShown()) {
        const std::shared_ptr<Change> change = pending;
        change->options.loading->update(engine, delta, getLoadingProgress(*change));
    }
    if (canProcessTop(clock)) {
        getTopShared()->update(engine, static_cast<float>(clock.getDelta()));
    }
}

// Hooks and listeners may clear the stack or request other changes, so every step checks that its change is still the pending one after it runs them. The time of an update goes to the effect unless the change spent it waiting for its load.
void SceneManager::advance(float deltaSeconds) {
    float remaining = deltaSeconds;
    while (pending) {
        const std::shared_ptr<Change> change = pending;
        switch (change->phase) {
        case Phase::Start:
            start(change);
            break;
        case Phase::Load:
            if (!waitForLoad(change, remaining)) {
                return;
            }
            if (change->blocked) {
                remaining = 0.0F;
            }
            if (pending == change && change->failure) {
                takeNext();
                reportFailure(*change, change->from != nullptr);
                if (change->options.completion) {
                    change->options.completion({.outcome = Outcome::Failed, .error = change->failure});
                }
            } else if (pending == change) {
                switchScenes(change);
            }
            break;
        case Phase::Cover:
            change->elapsed += std::exchange(remaining, 0.0F);
            if (!isFinished(*change) && getProgress(*change) < change->options.transition.effect->getSwitchProgress()) {
                return;
            }
            coverScreen(change);
            break;
        case Phase::Hold:
            if (!waitForLoad(change, remaining)) {
                return;
            }
            if (change->blocked) {
                remaining = 0.0F;
            }
            if (pending == change && change->failure) {
                restoreScenes(change);
            } else if (pending == change) {
                revealScene(change);
            }
            break;
        case Phase::Reveal:
            change->elapsed += std::exchange(remaining, 0.0F);
            if (!change->exited && (isFinished(*change) || getProgress(*change) >= change->options.transition.effect->getExitProgress())) {
                change->exited = true;
                exitLeaving();
                break;
            }
            if (!isFinished(*change)) {
                return;
            }
            finish(change);
            break;
        }
    }
}

void SceneManager::start(const std::shared_ptr<Change>& change) {
    change->from = getTopShared();
    const std::shared_ptr<Scene>& scene = change->scene;
    if (scene && (find(*scene) || std::ranges::find(leaving, scene) != leaving.end())) {
        throw std::invalid_argument("The scene is already on the stack.");
    }
    if (const auto taken = std::ranges::find(preloaded, scene); scene && taken != preloaded.end()) {
        if (change->options.params.has_value()) {
            throw std::invalid_argument("A preloaded scene keeps the params of its preload.");
        }
        preloaded.erase(taken);
    }

    // A pop that leaves the stack as it is ends at once.
    const std::size_t keep = getKeptLevel(*change);
    if (!scene && keep == stack.size()) {
        takeNext();
        if (change->options.completion) {
            change->options.completion({});
        }
        return;
    }

    const TransitionEffect* effect = change->options.transition.effect.get();
    change->covering = effect != nullptr && effect->getSwitchProgress() > 0.0F;
    if (!change->covering) {
        change->phase = Phase::Load;
        if (needsLoad(scene.get())) {
            startLoad(scene, std::move(change->options.params));
        }
        return;
    }

    // The scenes that leave the stack start exiting, the top scene hears that the transition takes it off the screen, and the next scene loads during the cover unless the replaced scene unloads first.
    prepareImages();
    change->phase = Phase::Cover;
    for (std::size_t level = keep; level < stack.size(); ++level) {
        stack[level]->state = Scene::State::Exiting;
    }
    if (change->from) {
        change->from->exitTransitionStarted(engine);
        if (pending != change) {
            return;
        }
        publish(LifecycleEvent::kSceneExitTransitionStarted, *change->from);
        if (pending != change) {
            return;
        }
    }
    publishPhase(LifecycleEvent::kSceneCoverStarted, *change);
    const bool unloadFirst = change->operation == Operation::Replace && change->options.unloadBeforeLoad && change->from;
    if (pending == change && !unloadFirst && needsLoad(scene.get())) {
        startLoad(scene, std::move(change->options.params));
    }
}

bool SceneManager::waitForLoad(const std::shared_ptr<Change>& change, float deltaSeconds) {
    const bool loaded = change->failure || !change->scene || change->scene->state == Scene::State::Loaded;
    if (change->viewShown) {
        change->shown += deltaSeconds;
    }
    if (change->fading) {
        change->faded += deltaSeconds;
    }
    if (!loaded) {
        change->waited += deltaSeconds;
        change->blocked = true;
        if (change->options.loading && !change->viewShown && change->waited >= change->options.loadingDelay) {
            showLoadingView(change);
        }
        return false;
    }

    // A view that appeared stays for its minimum time and then fades out into the covered frame, unless the load failed.
    if (change->viewShown && !change->failure) {
        if (change->shown < change->options.minimumLoadingTime) {
            change->blocked = true;
            return false;
        }
        if (change->phase == Phase::Hold && change->faded < change->options.loadingFadeOut) {
            change->fading = true;
            change->blocked = true;
            return false;
        }
    }
    if (change->viewShown) {
        hideLoadingView(*change);
    }
    return true;
}

void SceneManager::showLoadingView(const std::shared_ptr<Change>& change) {
    change->viewShown = true;
    change->shown = 0.0F;
    change->options.loading->enter(engine);
}

void SceneManager::hideLoadingView(Change& change) {
    change.viewShown = false;
    change.fading = false;
    change.options.loading->exit(engine);
}

float SceneManager::getLoadingViewOpacity() const noexcept {
    if (!isLoadingViewShown()) {
        return 0.0F;
    }
    if (!pending->fading) {
        return 1.0F;
    }
    return std::clamp(1.0F - pending->faded / pending->options.loadingFadeOut, 0.0F, 1.0F);
}

SceneLoad::Progress SceneManager::getLoadingProgress(const Change& change) {
    return change.scene ? change.scene->getLoadProgress() : SceneLoad::Progress{.value = 1.0F};
}

// At full cover the stack changes: a pushed-over scene is covered, a replaced scene exits and unloads before the next scene loads or waits in the leaving list, and popped scenes exit and unload while the scene below them resumes.
void SceneManager::coverScreen(const std::shared_ptr<Change>& change) {
    publishPhase(LifecycleEvent::kSceneCoverFinished, *change);
    if (pending != change) {
        return;
    }

    // A load that already failed changes nothing, and the hold reveals the scenes on the stack again.
    if (!change->failure) {
        const std::size_t keep = getKeptLevel(*change);
        switch (change->operation) {
        case Operation::Push:
            if (change->from) {
                coverScene(change->from);
            }
            break;
        case Operation::Replace:
            if (change->from) {
                stack.pop_back();
                release(change->from, change->options.unloadBeforeLoad);
            }
            if (pending == change && needsLoad(change->scene.get())) {
                startLoad(change->scene, std::move(change->options.params));
            }
            break;
        case Operation::Pop:
        case Operation::PopTo:
            while (stack.size() > keep && pending == change) {
                const std::shared_ptr<Scene> popped = stack.back();
                stack.pop_back();
                release(popped, true);
            }
            if (const std::shared_ptr<Scene> top = getTopShared(); top && pending == change) {
                uncoverScene(top);
            }
            break;
        }
        if (pending != change) {
            return;
        }
    }
    change->phase = Phase::Hold;
    publishPhase(LifecycleEvent::kSceneHoldStarted, *change);
}

void SceneManager::revealScene(const std::shared_ptr<Change>& change) {
    publishPhase(LifecycleEvent::kSceneHoldFinished, *change);
    if (pending != change) {
        return;
    }
    exitLeaving();
    if (pending != change) {
        return;
    }
    if (change->scene) {
        enterScene(change->scene);
        if (pending != change) {
            return;
        }
    }
    change->phase = Phase::Reveal;
    publishPhase(LifecycleEvent::kSceneRevealStarted, *change);
}

// The scene that was on top comes back when it is still alive, whether the cover covered it, left it waiting to exit or never reached it, and otherwise the scene below the replaced one does.
void SceneManager::restoreScenes(const std::shared_ptr<Change>& change) {
    publishPhase(LifecycleEvent::kSceneHoldFinished, *change);
    if (pending != change) {
        return;
    }

    const std::shared_ptr<Scene>& from = change->from;
    const bool waiting = from && std::ranges::find(leaving, from) != leaving.end();
    const bool kept = waiting || (from && find(*from));
    if (waiting) {
        std::erase(leaving, from);
        stack.push_back(from);
    }
    if (const std::shared_ptr<Scene> top = getTopShared(); top && top->state == Scene::State::Covered) {
        uncoverScene(top);
    } else if (top) {
        top->state = Scene::State::Entering;
    }
    if (pending != change) {
        return;
    }
    reportFailure(*change, kept);
    if (pending != change) {
        return;
    }
    change->phase = Phase::Reveal;
    publishPhase(LifecycleEvent::kSceneRevealStarted, *change);
}

// Without an effect the stack changes at once, and an effect that shows both scenes starts with the outgoing scenes alive in the outgoing image until its exit point.
void SceneManager::switchScenes(const std::shared_ptr<Change>& change) {
    const TransitionEffect* effect = change->options.transition.effect.get();
    if (effect != nullptr) {
        prepareImages();
        outgoing = getVisible(stack);
        change->exited = false;
    }
    if (change->from) {
        change->from->exitTransitionStarted(engine);
        if (pending != change) {
            return;
        }
        publish(LifecycleEvent::kSceneExitTransitionStarted, *change->from);
        if (pending != change) {
            return;
        }
    }

    const bool exitNow = effect == nullptr;
    const std::size_t keep = getKeptLevel(*change);
    switch (change->operation) {
    case Operation::Push:
        if (change->from) {
            coverScene(change->from);
        }
        break;
    case Operation::Replace:
        if (change->from) {
            stack.pop_back();
            release(change->from, exitNow);
        }
        break;
    case Operation::Pop:
    case Operation::PopTo:
        while (stack.size() > keep && pending == change) {
            const std::shared_ptr<Scene> popped = stack.back();
            stack.pop_back();
            release(popped, exitNow);
        }
        if (const std::shared_ptr<Scene> top = getTopShared(); top && pending == change) {
            uncoverScene(top);
        }
        break;
    }
    if (pending != change) {
        return;
    }
    if (change->scene) {
        enterScene(change->scene);
        if (pending != change) {
            return;
        }
    }
    if (effect == nullptr) {
        finish(change);
        return;
    }
    change->phase = Phase::Reveal;
    publishPhase(LifecycleEvent::kSceneRevealStarted, *change);
}

void SceneManager::finish(const std::shared_ptr<Change>& change) {
    if (change->options.transition.effect) {
        publishPhase(LifecycleEvent::kSceneRevealFinished, *change);
        if (pending != change) {
            return;
        }
    }

    // The next change is already pending when the hooks run, so changes they request queue behind it.
    takeNext();
    if (const std::shared_ptr<Scene> top = getTopShared(); top && top->state == Scene::State::Entering) {
        top->state = Scene::State::Active;
        top->enterTransitionFinished(engine);
        publish(LifecycleEvent::kSceneEnterTransitionFinished, *top);
    }
    if (change->options.completion) {
        change->options.completion(change->failure ? Result{.outcome = Outcome::Failed, .error = change->failure} : Result{});
    }
}

void SceneManager::takeNext() noexcept {
    endTransition();
    pending.reset();
    if (!queue.empty()) {
        pending = std::move(queue.front());
        queue.pop_front();
    }
}

// An error handler of the change takes the failure. Otherwise the log reports it while a scene the change kept stays on screen, and the error screen shows it when no such scene is left.
void SceneManager::reportFailure(const Change& change, bool currentKept) {
    const lua::Error& error = *change.failure;
    if (change.options.onError) {
        change.options.onError(error);
        return;
    }
    if (currentKept) {
        Log::error("A scene could not load: {}", error.what());
        return;
    }
    engine.reportError(error);
}

// A hook that clears the stack takes its scene away, so the event is published only for a scene that is still there.
void SceneManager::enterScene(const std::shared_ptr<Scene>& scene) {
    const std::shared_ptr<SceneLoad> load = std::exchange(scene->loading, {});
    stack.push_back(scene);
    scene->state = Scene::State::Entering;
    scene->enter(engine, load->getParams());
    if (scene->state == Scene::State::Entering) {
        publish(LifecycleEvent::kSceneEntered, *scene);
    }
}

void SceneManager::exitScene(const std::shared_ptr<Scene>& scene) {
    scene->state = Scene::State::Exiting;
    scene->exit(engine);
    scene->state = Scene::State::Exited;
    publish(LifecycleEvent::kSceneExited, *scene);
}

// A scene that unloads stops its load when it still runs, and everything it owns ends even when its hook fails.
void SceneManager::unloadScene(const std::shared_ptr<Scene>& scene) {
    if (const std::shared_ptr<SceneLoad> load = std::exchange(scene->loading, {})) {
        load->cancel();
    }
    std::exception_ptr failure;
    try {
        scene->unload(engine);
    } catch (...) {
        failure = std::current_exception();
    }
    scene->connections.clear();
    scene->state = Scene::State::Unloaded;
    if (failure) {
        std::rethrow_exception(failure);
    }
    publish(LifecycleEvent::kSceneUnloaded, *scene);
}

// A scene that leaves unloads even when its exit fails, so what it owns always ends, and the first failure is raised again.
void SceneManager::retire(const std::shared_ptr<Scene>& scene) {
    std::exception_ptr failure;
    try {
        exitScene(scene);
    } catch (...) {
        failure = std::current_exception();
    }
    try {
        unloadScene(scene);
    } catch (...) {
        if (!failure) {
            failure = std::current_exception();
        }
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void SceneManager::coverScene(const std::shared_ptr<Scene>& scene) {
    scene->state = Scene::State::Covered;
    scene->pause(engine);
    if (scene->state == Scene::State::Covered) {
        publish(LifecycleEvent::kScenePaused, *scene);
    }
}

void SceneManager::uncoverScene(const std::shared_ptr<Scene>& scene) {
    scene->state = Scene::State::Entering;
    scene->resume(engine);
    if (scene->state == Scene::State::Entering) {
        publish(LifecycleEvent::kSceneResumed, *scene);
    }
}

void SceneManager::release(const std::shared_ptr<Scene>& scene, bool exitNow) {
    scene->state = Scene::State::Exiting;
    if (exitNow) {
        retire(scene);
        return;
    }
    leaving.push_back(scene);
}

// Leaving scenes exit in the order they left, from the top down, and every one leaves even when another one fails.
void SceneManager::exitLeaving() {
    const std::vector<std::shared_ptr<Scene>> exiting = std::exchange(leaving, {});
    outgoing.clear();
    std::exception_ptr failure;
    for (const std::shared_ptr<Scene>& scene : exiting) {
        try {
            retire(scene);
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

float SceneManager::getProgress(const Change& change) const {
    const Transition& transition = change.options.transition;
    const float fraction = transition.duration > 0.0F ? std::min(change.elapsed / transition.duration, 1.0F) : 1.0F;
    return transition.ease.apply(fraction);
}

bool SceneManager::isFinished(const Change& change) const {
    return change.options.transition.duration <= 0.0F || change.elapsed >= change.options.transition.duration;
}

void SceneManager::event(const platform::Event& event) {
    const std::shared_ptr<Scene> scene = getTopShared();
    if (!scene) {
        return;
    }

    // Input reaches the top scene only while no change holds it and the scene runs, and every other event always does.
    if (event.isInput() && (isInputBlocked() || !canProcessTop(engine.getClock()))) {
        return;
    }
    scene->event(engine, event);
}

void SceneManager::fixedUpdate(const FrameClock& clock) {
    if (canProcessTop(clock)) {
        getTopShared()->fixedUpdate(engine, static_cast<float>(clock.getFixedStep()));
    }
}

std::vector<std::shared_ptr<Scene>> SceneManager::getVisible(const std::vector<std::shared_ptr<Scene>>& scenes) {
    std::size_t first = scenes.size();
    while (first > 0) {
        --first;
        if (!scenes[first]->isTransparent()) {
            break;
        }
    }
    return {scenes.begin() + static_cast<std::ptrdiff_t>(first), scenes.end()};
}

bool SceneManager::isAlive(const Scene& scene) const noexcept {
    const auto matches = [&scene](const std::shared_ptr<Scene>& candidate) { return candidate.get() == &scene; };
    return std::any_of(stack.begin(), stack.end(), matches) || std::any_of(leaving.begin(), leaving.end(), matches);
}

bool SceneManager::isEffectShown() const noexcept {
    return pending && pending->options.transition.effect && (pending->phase == Phase::Cover || pending->phase == Phase::Hold || pending->phase == Phase::Reveal);
}

// Both images have the pixel size of the viewport, so a captured frame looks the same as the screen would.
void SceneManager::prepareImages() {
    const math::Rect& pixels = engine.getViewport().getPixelRect();
    const int width = std::max(1, static_cast<int>(std::lround(pixels.width)));
    const int height = std::max(1, static_cast<int>(std::lround(pixels.height)));
    if (outgoingImage.isValid() && outgoingImage.getWidth() == width && outgoingImage.getHeight() == height) {
        return;
    }
    const graphics::Texture::Options options{.filter = graphics::Texture::Filter::Linear};
    outgoingImage = engine.getGraphics().createRenderTarget(width, height, options);
    incomingImage = engine.getGraphics().createRenderTarget(width, height, options);
}

// The image of a fading loading view has the size of the other images, which the views of the frame prepared.
void SceneManager::prepareLoadingImage() {
    if (loadingImage.isValid() && loadingImage.getWidth() == outgoingImage.getWidth() && loadingImage.getHeight() == outgoingImage.getHeight()) {
        return;
    }
    loadingImage = engine.getGraphics().createRenderTarget(outgoingImage.getWidth(), outgoingImage.getHeight(), {.filter = graphics::Texture::Filter::Linear});
}

void SceneManager::endTransition() noexcept {
    outgoing.clear();
    outgoingImage = {};
    incomingImage = {};
    loadingImage = {};
}

// The cover renders the current scenes into the outgoing image with the incoming image empty, the hold draws the covered frame with the loading view on top and no scene renders, and the reveal renders the scenes after the change into the incoming image, next to the scenes still leaving through an effect that shows both scenes.
std::vector<SceneManager::View> SceneManager::getViews() {
    if (!isEffectShown()) {
        return {View{.scenes = getVisible(stack), .current = true}};
    }

    prepareImages();
    const Change& change = *pending;
    std::vector<View> views;
    switch (change.phase) {
    case Phase::Cover:
        views.push_back({.scenes = getVisible(stack), .target = outgoingImage, .current = true});
        views.push_back({.target = incomingImage});
        views.push_back({.effect = true});
        break;
    case Phase::Hold:
        if (change.fading) {
            prepareLoadingImage();
            views.push_back({.target = loadingImage, .current = true, .effect = true});
            views.push_back({.effect = true});
            break;
        }
        views.push_back({.current = true, .effect = true});
        break;
    case Phase::Start:
    case Phase::Load:
    case Phase::Reveal:
        if (!change.exited) {
            views.push_back({.scenes = outgoing, .target = outgoingImage});
        }
        views.push_back({.scenes = getVisible(stack), .target = incomingImage, .current = true});
        views.push_back({.effect = true});
        break;
    }
    return views;
}

// A scene that leaves while an earlier one renders, through a clear, draws nothing more, and the loading view draws above the scenes of the current view.
void SceneManager::render(const View& view) {
    for (const std::shared_ptr<Scene>& scene : view.scenes) {
        if (isAlive(*scene)) {
            scene->render(engine);
        }
    }
    if (view.current && isLoadingViewShown()) {
        const std::shared_ptr<Change> change = pending;
        change->options.loading->render(engine, getLoadingProgress(*change));
    }
}

void SceneManager::renderUi(const View& view) {
    for (const std::shared_ptr<Scene>& scene : view.scenes) {
        if (isAlive(*scene)) {
            scene->renderUi(engine);
        }
    }
    if (view.current && isLoadingViewShown()) {
        const std::shared_ptr<Change> change = pending;
        change->options.loading->renderUi(engine, getLoadingProgress(*change));
    }
}

// The hold draws the effect at its switch progress, where it covers the screen, and the reveal of an effect that covers the screen never goes back below it.
void SceneManager::renderTransition(graphics2d::Renderer& renderer, const View& view) {
    if (!isEffectShown()) {
        return;
    }
    const Change& change = *pending;
    const std::shared_ptr<TransitionEffect> effect = change.options.transition.effect;
    float progress = getProgress(change);
    if (change.phase == Phase::Hold) {
        progress = effect->getSwitchProgress();
    } else if (change.covering && change.phase == Phase::Reveal) {
        progress = std::max(progress, effect->getSwitchProgress());
    }
    effect->render(renderer, {.outgoing = outgoingImage.getTexture(), .incoming = incomingImage.getTexture()}, progress);
    if (!change.fading || view.current) {
        return;
    }

    // The loading view that fades out rendered over the covered frame into its own image, which now blends over the covered frame of the screen.
    renderer.beginScreen();
    const math::Rect area = renderer.getCanvasBounds();
    const graphics::Texture& image = loadingImage.getTexture();
    renderer.draw({.texture = image, .source = {0.0F, 0.0F, image.getSize().x, image.getSize().y}, .position = area.getMin(), .size = area.getSize(), .pivot = {}, .color = math::Color::white().withAlpha(getLoadingViewOpacity())});
}

} // namespace haylen::core
