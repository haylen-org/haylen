#include <gtest/gtest.h>

#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/LoadingView.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"
#include "support/RecordingEffect.hpp"
#include "support/RecordingScene.hpp"
#include "support/TestFiles.hpp"

namespace haylen::core {

namespace {

// Writes the scene events of the engine to a log next to the hooks of the scenes, as the event name followed by the scenes it carries.
class EventRecorder final {
  public:
    EventRecorder(Engine& engine, std::vector<std::string>& entries) : log(entries) {
        for (const std::string_view name : {LifecycleEvent::kSceneLoading, LifecycleEvent::kSceneLoaded, LifecycleEvent::kSceneEntered, LifecycleEvent::kSceneExited, LifecycleEvent::kSceneUnloaded, LifecycleEvent::kScenePaused, LifecycleEvent::kSceneResumed, LifecycleEvent::kSceneExitTransitionStarted, LifecycleEvent::kSceneEnterTransitionFinished}) {
            connections.push_back(engine.getEvents().on(name, [this, name](EventBus::Event& event) { log.push_back(std::string(name) + " " + describe(event.get<Scene>())); }));
        }
        for (const std::string_view name : {LifecycleEvent::kSceneCoverStarted, LifecycleEvent::kSceneCoverFinished, LifecycleEvent::kSceneHoldStarted, LifecycleEvent::kSceneHoldFinished, LifecycleEvent::kSceneRevealStarted, LifecycleEvent::kSceneRevealFinished}) {
            // clang-format off
            connections.push_back(engine.getEvents().on(name, [this, name](EventBus::Event& event) {
                const SceneManager::Transfer& transfer = *event.get<SceneManager::Transfer>();
                log.push_back(std::string(name) + " " + describe(transfer.from) + ">" + describe(transfer.to));
            }));
            // clang-format on
        }
        // clang-format off
        connections.push_back(engine.getEvents().on(LifecycleEvent::kSceneLoadFailed, [this](EventBus::Event& event) {
            const SceneManager::LoadFailure& failure = *event.get<SceneManager::LoadFailure>();
            log.push_back("sceneLoadFailed " + describe(failure.scene) + " " + failure.error->what());
        }));
        // clang-format on
    }

  private:
    [[nodiscard]] static std::string describe(const Scene* scene) {
        const auto* recorded = dynamic_cast<const test::RecordingScene*>(scene);
        return recorded != nullptr ? recorded->getName() : "none";
    }

    std::vector<std::string>& log;
    std::vector<Connection> connections;
};

// Scene whose load runs a function of the test with the load context.
class LoadScene final : public Scene {
  public:
    explicit LoadScene(std::function<void(SceneLoad&)> body) : run(std::move(body)) {}

    void load(Engine&, SceneLoad& context) override {
        run(context);
    }

  private:
    std::function<void(SceneLoad&)> run;
};

// Loading view that writes its hooks to a log and keeps the last progress it received.
class RecordingView final : public LoadingView {
  public:
    explicit RecordingView(std::vector<std::string>& entries) : log(entries) {}

    void enter(Engine&) override {
        log.emplace_back("view:enter");
    }
    void exit(Engine&) override {
        log.emplace_back("view:exit");
    }
    void update(Engine&, float, const SceneLoad::Progress& progress) override {
        ++updates;
        last = progress;
    }
    void render(Engine&, const SceneLoad::Progress&) override {
        ++renders;
    }
    void renderUi(Engine&, const SceneLoad::Progress&) override {
        ++uiRenders;
    }

    int updates = 0;
    int renders = 0;
    int uiRenders = 0;
    SceneLoad::Progress last;

  private:
    std::vector<std::string>& log;
};

} // namespace

TEST(SceneLoadTest, FinishesOnceTheHookAndEveryDeferralReleasedIt) {
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    std::optional<SceneLoad::Deferral> first;
    std::optional<SceneLoad::Deferral> second;
    SceneLoad* context = nullptr;
    // clang-format off
    auto scene = std::make_shared<LoadScene>([&](SceneLoad& load) {
        context = &load;
        load.setProgress(0.25F, "reading");
        first.emplace(load.defer());
        second.emplace(load.defer());
    });
    // clang-format on
    scenes.preload(scene, std::string("params"));
    ASSERT_NE(context, nullptr);
    EXPECT_EQ(std::any_cast<std::string>(context->getParams()), "params");
    EXPECT_EQ(scene->getLoadProgress().value, 0.25F);
    EXPECT_EQ(scene->getLoadProgress().message, "reading");
    EXPECT_THROW(context->setProgress(1.5F), std::invalid_argument);

    // The load waits for every deferral, whichever object holds it by then.
    first->complete();
    first->complete();
    fixture.frames(1);
    EXPECT_EQ(scene->getState(), Scene::State::Loading);
    SceneLoad::Deferral moved;
    moved = std::move(*second);
    moved.complete();
    fixture.frames(1);
    EXPECT_EQ(scene->getState(), Scene::State::Loaded);
    EXPECT_EQ(scene->getLoadProgress().value, 1.0F);
    EXPECT_THROW((void)context->defer(), std::logic_error);

    // A deferral that goes away before it completed fails the load, so a load never waits for dropped work.
    std::optional<SceneManager::Result> result;
    scenes.preload(std::make_shared<LoadScene>([](SceneLoad& load) { const SceneLoad::Deferral forgotten = load.defer(); }), {}, [&](const SceneManager::Result& value) { result = value; });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, SceneManager::Outcome::Failed);
    EXPECT_STREQ(result->error->what(), "The scene load was dropped before it finished.");

    // A group the asset manager does not know fails the load with the reason of the manager.
    result.reset();
    scenes.preload(std::make_shared<LoadScene>([](SceneLoad& load) { load.preload("missing"); }), {}, [&](const SceneManager::Result& value) { result = value; });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, SceneManager::Outcome::Failed);
    EXPECT_STREQ(result->error->what(), "Unknown asset group: missing");
}

TEST(SceneManagerTest, UsesTheDefaultHooksOfALoadingView) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto slow = std::make_shared<test::RecordingScene>("slow", log);
    slow->loading = test::RecordingScene::Load::Deferred;
    scenes.push(slow, {.transition = SceneManager::Transition::fade(0.2F), .loading = std::make_shared<LoadingView>()});
    fixture.frames(3, 0.1);
    EXPECT_TRUE(scenes.isLoadingViewShown());
    slow->deferral->complete();
    fixture.frames(5, 0.1);
    EXPECT_EQ(slow->getState(), Scene::State::Active);
}

TEST(SceneManagerTest, PushesReplacesAndPopsThroughTheLifecycle) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto first = std::make_shared<test::RecordingScene>("first", log);
    auto overlay = std::make_shared<test::RecordingScene>("overlay", log, true);
    auto second = std::make_shared<test::RecordingScene>("second", log);
    EXPECT_EQ(first->getState(), Scene::State::Created);

    scenes.push(first);
    fixture.frames(1);
    EXPECT_EQ(first->getState(), Scene::State::Active);
    scenes.push(overlay);
    fixture.frames(1);
    EXPECT_EQ(first->getState(), Scene::State::Covered);
    EXPECT_EQ(scenes.size(), 2U);
    EXPECT_EQ(scenes.getTop(), overlay.get());

    // A covered scene keeps rendering below a transparent scene, but only the top scene updates.
    fixture.frames(1);
    EXPECT_EQ(first->updates, 1);
    EXPECT_EQ(first->renders, 3);

    scenes.pop();
    fixture.frames(1);
    EXPECT_EQ(overlay->getState(), Scene::State::Unloaded);
    scenes.replace(second);
    fixture.frames(1);

    EXPECT_EQ(log, (std::vector<std::string>{"first:load", "first:enter", "first:enterTransitionFinished", "overlay:load", "first:exitTransitionStarted", "first:pause", "overlay:enter", "overlay:enterTransitionFinished", "overlay:exitTransitionStarted", "overlay:exit", "overlay:unload", "first:resume", "first:enterTransitionFinished", "second:load", "first:exitTransitionStarted", "first:exit", "first:unload", "second:enter", "second:enterTransitionFinished"}));

    scenes.clear();
    EXPECT_TRUE(scenes.empty());
    EXPECT_EQ(scenes.getTop(), nullptr);
    EXPECT_EQ(log.back(), "second:unload");
    EXPECT_EQ(second->getState(), Scene::State::Unloaded);

    // A scene that unloaded can come back, and it loads again.
    log.clear();
    scenes.push(overlay);
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{"overlay:load", "overlay:enter", "overlay:enterTransitionFinished"}));

    scenes.popTo(3);
    scenes.pop();
    scenes.pop();
    fixture.frames(1);
    EXPECT_TRUE(scenes.empty());
    EXPECT_THROW(scenes.push(nullptr), std::invalid_argument);
    EXPECT_THROW(scenes.replace(nullptr), std::invalid_argument);
    EXPECT_THROW(scenes.preload(nullptr), std::invalid_argument);
}

TEST(SceneManagerTest, CoversHoldsWhileTheNextSceneLoadsAndReveals) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto menu = std::make_shared<test::RecordingScene>("menu", log);
    scenes.push(menu);
    fixture.frames(1);

    const EventRecorder events(fixture.engine(), log);
    auto effect = std::make_shared<test::RecordingEffect>(0.5F, 0.5F);
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    std::vector<SceneManager::Outcome> outcomes;
    scenes.replace(game, {.transition = {.duration = 1.0F, .effect = effect}, .params = std::string("level 3"), .completion = [&](const SceneManager::Result& result) { outcomes.push_back(result.outcome); }});
    log.clear();

    // The cover takes the menu off the screen while it still updates.
    const int updates = menu->updates;
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"menu:exitTransitionStarted", "sceneExitTransitionStarted menu", "sceneCoverStarted menu>game"}));
    EXPECT_EQ(menu->getState(), Scene::State::Exiting);
    EXPECT_EQ(menu->updates, updates + 1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 3U);

    // At full cover the menu leaves before the game loads, and the hold draws the covered frame alone.
    log.clear();
    const int renders = menu->renders;
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"sceneCoverFinished menu>game", "menu:exit", "sceneExited menu", "menu:unload", "sceneUnloaded menu", "sceneLoading game", "game:load", "sceneHoldStarted menu>game"}));
    EXPECT_EQ(menu->getState(), Scene::State::Unloaded);
    EXPECT_EQ(game->getState(), Scene::State::Loading);
    EXPECT_TRUE(scenes.empty());
    EXPECT_EQ(menu->renders, renders);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 1U);
    fixture.frames(3, 0.25);
    EXPECT_EQ(effect->progresses.back(), 0.5F);
    EXPECT_EQ(std::any_cast<std::string>(game->loadParams), "level 3");

    // Once loaded, the game enters and the reveal plays the rest of the effect.
    log.clear();
    game->deferral->complete();
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"sceneLoaded game", "sceneHoldFinished menu>game", "game:enter", "sceneEntered game", "sceneRevealStarted menu>game"}));
    EXPECT_EQ(game->getState(), Scene::State::Entering);
    EXPECT_EQ(std::any_cast<std::string>(game->enterParams), "level 3");
    EXPECT_EQ(effect->progresses.back(), 0.5F);
    fixture.frames(1, 0.25);
    EXPECT_FLOAT_EQ(effect->progresses.back(), 0.75F);
    EXPECT_TRUE(outcomes.empty());

    log.clear();
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"sceneRevealFinished menu>game", "game:enterTransitionFinished", "sceneEnterTransitionFinished game"}));
    EXPECT_EQ(game->getState(), Scene::State::Active);
    EXPECT_EQ(outcomes, std::vector<SceneManager::Outcome>{SceneManager::Outcome::Completed});
    EXPECT_FALSE(scenes.isTransitioning());
}

TEST(SceneManagerTest, KeepsTheReplacedSceneUntilTheNextOneLoadedWhenAsked) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    scenes.push(std::make_shared<test::RecordingScene>("menu", log));
    fixture.frames(1);

    // The load starts with the cover, and the menu waits to exit until the game loaded.
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(game, {.transition = SceneManager::Transition::fade(1.0F), .unloadBeforeLoad = false});
    log.clear();
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"menu:exitTransitionStarted", "game:load"}));
    fixture.frames(2, 0.25);
    EXPECT_EQ(log.size(), 2U);
    EXPECT_EQ(scenes.getTop(), nullptr);

    game->deferral->complete();
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"menu:exitTransitionStarted", "game:load", "menu:exit", "menu:unload", "game:enter"}));

    // A pushed-over scene only pauses at full cover and comes back when the pushed scene pops.
    fixture.frames(2, 0.25);
    log.clear();
    scenes.push(std::make_shared<test::RecordingScene>("map", log), {.transition = SceneManager::Transition::fade(0.5F)});
    fixture.frames(4, 0.125);
    scenes.pop(SceneManager::Transition::fade(0.5F));
    fixture.frames(4, 0.125);
    EXPECT_EQ(log, (std::vector<std::string>{"game:exitTransitionStarted", "map:load", "game:pause", "map:enter", "map:enterTransitionFinished", "map:exitTransitionStarted", "map:exit", "map:unload", "game:resume", "game:enterTransitionFinished"}));
}

TEST(SceneManagerTest, LoadsFirstForEffectsThatShowBothScenes) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto menu = std::make_shared<test::RecordingScene>("menu", log);
    scenes.push(menu);
    fixture.frames(1);

    // While the game loads, the menu stays on the screen and keeps running, and the effect has not started.
    auto effect = std::make_shared<test::RecordingEffect>(0.0F, 1.0F);
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(game, {.transition = {.duration = 1.0F, .effect = effect}});
    log.clear();
    const int updates = menu->updates;
    fixture.frames(3, 0.25);
    EXPECT_EQ(log, std::vector<std::string>{"game:load"});
    EXPECT_EQ(menu->getState(), Scene::State::Active);
    EXPECT_EQ(menu->updates, updates + 3);
    EXPECT_TRUE(effect->progresses.empty());
    EXPECT_TRUE(scenes.isInputBlocked());
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 1U);

    // Then both scenes play the effect alive, each in its image, and the menu exits at the exit point.
    game->deferral->complete();
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"game:load", "menu:exitTransitionStarted", "game:enter"}));
    EXPECT_EQ(menu->getState(), Scene::State::Exiting);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 3U);
    ASSERT_TRUE(effect->last.outgoing.isValid());
    EXPECT_NE(effect->last.outgoing, effect->last.incoming);
    const int renders = menu->renders;
    fixture.frames(3, 0.25);
    EXPECT_EQ(menu->renders, renders + 3);
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"game:load", "menu:exitTransitionStarted", "game:enter", "menu:exit", "menu:unload", "game:enterTransitionFinished"}));
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 1U);
}

TEST(SceneManagerTest, LoadsBeforeSwitchingWithoutATransition) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto menu = std::make_shared<test::RecordingScene>("menu", log);
    scenes.push(menu);
    fixture.frames(1);

    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(game);
    log.clear();
    fixture.frames(5);
    EXPECT_EQ(log, std::vector<std::string>{"game:load"});
    EXPECT_EQ(scenes.getTop(), menu.get());
    EXPECT_TRUE(scenes.isInputBlocked());

    game->deferral->complete();
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{"game:load", "menu:exitTransitionStarted", "menu:exit", "menu:unload", "game:enter", "game:enterTransitionFinished"}));
    EXPECT_FALSE(scenes.isTransitioning());
}

TEST(SceneManagerTest, ShowsTheLoadingViewAfterItsDelayForItsMinimumTime) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    scenes.push(std::make_shared<test::RecordingScene>("menu", log));
    fixture.frames(1);

    // A slow load shows the view over the covered screen once the delay passed.
    auto view = std::make_shared<RecordingView>(log);
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(game, {.transition = SceneManager::Transition::fade(0.5F), .loading = view, .loadingDelay = 0.2F, .minimumLoadingTime = 0.5F});
    fixture.frames(2, 0.125);
    fixture.frames(1, 0.125);
    EXPECT_FALSE(scenes.isLoadingViewShown());
    fixture.frames(1, 0.125);
    EXPECT_TRUE(scenes.isLoadingViewShown());
    EXPECT_EQ(log.back(), "view:enter");

    // The view draws above the covered frame, before the plugins, and receives the progress of the load.
    fixture.frames(2, 0.125);
    EXPECT_EQ(view->renders, 3);
    EXPECT_EQ(view->uiRenders, 3);
    EXPECT_EQ(view->updates, 3);
    EXPECT_EQ(view->last.value, 0.0F);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 1U);

    // A load that finished early still shows the view for its minimum time.
    game->deferral->complete();
    fixture.frames(1, 0.125);
    EXPECT_TRUE(scenes.isLoadingViewShown());
    EXPECT_EQ(view->last.value, 1.0F);

    // Then the view fades out into the covered frame, rendering into an image of its own that blends over it.
    fixture.frames(1, 0.125);
    EXPECT_EQ(scenes.getLoadingViewOpacity(), 1.0F);
    fixture.frames(1, 0.125);
    EXPECT_EQ(scenes.getLoadingViewOpacity(), 0.5F);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 2U);
    fixture.frames(1, 0.125);
    EXPECT_FALSE(scenes.isLoadingViewShown());
    EXPECT_EQ(scenes.getLoadingViewOpacity(), 0.0F);
    EXPECT_EQ(log.back(), "game:enter");
    EXPECT_EQ(log[log.size() - 2], "view:exit");

    // Without a fade-out the view goes away as soon as the load is done.
    auto cave = std::make_shared<test::RecordingScene>("cave", log);
    cave->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(cave, {.transition = SceneManager::Transition::fade(0.5F), .loading = view, .loadingFadeOut = 0.0F});
    ASSERT_TRUE(fixture.frameUntil([&scenes] { return scenes.isLoadingViewShown(); }));
    cave->deferral->complete();
    fixture.frames(1, 0.125);
    EXPECT_FALSE(scenes.isLoadingViewShown());
    EXPECT_EQ(log.back(), "cave:enter");
    EXPECT_THROW(scenes.push(std::make_shared<test::RecordingScene>("late", log), {.loadingFadeOut = -1.0F}), std::invalid_argument);

    // A quick load never shows the view, and without an effect the view shows over the current scenes.
    auto quick = std::make_shared<test::RecordingScene>("quick", log);
    scenes.replace(quick, {.loading = view, .loadingDelay = 0.2F});
    auto slow = std::make_shared<test::RecordingScene>("slow", log);
    slow->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(slow, {.loading = view});
    fixture.frames(4, 0.125);
    EXPECT_EQ(scenes.getTop(), quick.get());
    EXPECT_TRUE(scenes.isLoadingViewShown());
    const int quickRenders = quick->renders;
    fixture.frames(1);
    EXPECT_EQ(quick->renders, quickRenders + 1);
    EXPECT_EQ(std::count(log.begin(), log.end(), "view:enter"), 3);
    slow->deferral->complete();
    fixture.frames(1);
    EXPECT_EQ(scenes.getTop(), slow.get());
    EXPECT_FALSE(scenes.isLoadingViewShown());
}
TEST(SceneManagerTest, PreloadsAssetGroupsIntoTheProgressOfTheLoad) {
    std::vector<std::string> log;
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(4, 4, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/world/grass.png", std::string(image.begin(), image.end())}, {"content/world/stone.png", std::string(image.begin(), image.end())}});
    assets::Manager& assets = fixture.engine().getAssets();
    assets.defineGroup("world", {{.path = "world/"}});
    assets.defineGroup("broken", {{.path = "world/missing.png", .type = "texture"}});
    assets.setUploadBudget(0.0);
    SceneManager& scenes = fixture.engine().getScenes();

    // Each upload of the group moves the progress of the load, which ends once the group loaded.
    auto level = std::make_shared<test::RecordingScene>("level", log);
    level->groups = {"world"};
    scenes.push(level);
    ASSERT_TRUE(fixture.frameUntil([&] { return level->getLoadProgress().value > 0.0F; }));
    EXPECT_FLOAT_EQ(level->getLoadProgress().value, 0.5F);
    EXPECT_EQ(level->getState(), Scene::State::Loading);
    ASSERT_TRUE(fixture.frameUntil([&] { return level->getState() == Scene::State::Active; }));
    EXPECT_TRUE(assets.isGroupLoaded("world"));
    EXPECT_FLOAT_EQ(level->getLoadProgress().value, 1.0F);

    // An asset of a group that cannot load fails the load.
    auto broken = std::make_shared<test::RecordingScene>("broken", log);
    broken->groups = {"broken"};
    std::optional<SceneManager::Result> result;
    scenes.push(broken, {.completion = [&](const SceneManager::Result& value) { result = value; }});
    ASSERT_TRUE(fixture.frameUntil([&] { return result.has_value(); }));
    EXPECT_EQ(result->outcome, SceneManager::Outcome::Failed);
    EXPECT_NE(std::string(result->error->what()).find("The asset group broken could not load world/missing.png"), std::string::npos);
    EXPECT_EQ(scenes.getTop(), level.get());
    EXPECT_EQ(broken->getState(), Scene::State::Unloaded);
}

TEST(SceneManagerTest, FailedLoadsKeepTheCurrentSceneAndRejectTheChange) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto menu = std::make_shared<test::RecordingScene>("menu", log);
    scenes.push(menu);
    fixture.frames(1);
    const EventRecorder events(fixture.engine(), log);
    std::vector<std::string> lines;
    const std::uint64_t listener = Log::addListener([&lines](Log::Level, std::string_view line) { lines.emplace_back(line); });

    // A load that fails while the cover plays lets the cover finish and reveals the menu again.
    auto broken = std::make_shared<test::RecordingScene>("broken", log);
    broken->loading = test::RecordingScene::Load::Failing;
    std::vector<SceneManager::Result> results;
    scenes.push(broken, {.transition = SceneManager::Transition::fade(0.5F), .completion = [&](const SceneManager::Result& result) { results.push_back(result); }});
    log.clear();
    fixture.frames(4, 0.125);
    EXPECT_EQ(log, (std::vector<std::string>{"menu:exitTransitionStarted", "sceneExitTransitionStarted menu", "sceneCoverStarted menu>broken", "sceneLoading broken", "broken:load", "sceneLoadFailed broken broken cannot load", "broken:unload", "sceneUnloaded broken", "sceneCoverFinished menu>broken", "sceneHoldStarted menu>broken", "sceneHoldFinished menu>broken", "sceneRevealStarted menu>menu", "sceneRevealFinished menu>menu", "menu:enterTransitionFinished", "sceneEnterTransitionFinished menu"}));
    ASSERT_EQ(results.size(), 1U);
    EXPECT_EQ(results[0].outcome, SceneManager::Outcome::Failed);
    EXPECT_STREQ(results[0].error->what(), "broken cannot load");
    EXPECT_EQ(broken->getState(), Scene::State::Unloaded);
    EXPECT_EQ(menu->getState(), Scene::State::Active);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::find_if(lines.begin(), lines.end(), [](const std::string& line) { return line.find("A scene could not load: broken cannot load") != std::string::npos; }), lines.end());
    Log::removeListener(listener);

    // A load that fails during the hold brings back the menu, which waited to exit.
    auto late = std::make_shared<test::RecordingScene>("late", log);
    late->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(late, {.transition = SceneManager::Transition::fade(0.5F), .unloadBeforeLoad = false});
    fixture.frames(2, 0.125);
    EXPECT_EQ(menu->getState(), Scene::State::Exiting);
    log.clear();
    late->deferral->fail(lua::Error("late failed"));
    fixture.frames(3, 0.125);
    EXPECT_EQ(log, (std::vector<std::string>{"sceneLoadFailed late late failed", "late:unload", "sceneUnloaded late", "sceneHoldFinished menu>late", "sceneRevealStarted menu>menu", "sceneRevealFinished menu>menu", "menu:enterTransitionFinished", "sceneEnterTransitionFinished menu"}));
    EXPECT_EQ(scenes.getTop(), menu.get());
    EXPECT_EQ(menu->getState(), Scene::State::Active);
}

TEST(SceneManagerTest, FailedLoadsRestoreWhatIsLeftOnTheStack) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto base = std::make_shared<test::RecordingScene>("base", log);
    auto menu = std::make_shared<test::RecordingScene>("menu", log);
    scenes.push(base);
    scenes.push(menu);
    fixture.frames(1);

    // A push that fails during the hold hides its loading view at once and uncovers the menu, which it had covered.
    auto view = std::make_shared<RecordingView>(log);
    auto shop = std::make_shared<test::RecordingScene>("shop", log);
    shop->loading = test::RecordingScene::Load::Deferred;
    scenes.push(shop, {.transition = SceneManager::Transition::fade(0.5F), .loading = view, .minimumLoadingTime = 5.0F, .onError = [&](const lua::Error& error) { log.push_back(std::string("error ") + error.what()); }});
    fixture.frames(3, 0.125);
    EXPECT_EQ(menu->getState(), Scene::State::Covered);
    EXPECT_TRUE(scenes.isLoadingViewShown());
    log.clear();
    shop->deferral->fail(lua::Error("closed"));
    fixture.frames(3, 0.125);
    EXPECT_EQ(log, (std::vector<std::string>{"shop:unload", "view:exit", "menu:resume", "error closed", "menu:enterTransitionFinished"}));
    EXPECT_EQ(menu->getState(), Scene::State::Active);

    // A replace that fails after the menu unloaded uncovers the scene below it, while the failure reaches the error screen.
    auto cave = std::make_shared<test::RecordingScene>("cave", log);
    cave->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(cave, {.transition = SceneManager::Transition::fade(0.5F)});
    fixture.frames(2, 0.125);
    log.clear();
    cave->deferral->fail(lua::Error("flooded"));
    fixture.frames(1, 0.125);
    EXPECT_EQ(log, (std::vector<std::string>{"cave:unload", "base:resume"}));
    EXPECT_EQ(scenes.getTop(), base.get());
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "flooded");
}

TEST(SceneManagerTest, FailedLoadsAfterTheCurrentSceneLeftGoToTheErrorHandler) {
    std::vector<std::string> log;
    {
        // Without a handler, a failure after the menu unloaded leaves nothing to show but the error screen.
        test::EngineFixture fixture;
        SceneManager& scenes = fixture.engine().getScenes();
        scenes.push(std::make_shared<test::RecordingScene>("menu", log));
        fixture.frames(1);
        auto broken = std::make_shared<test::RecordingScene>("broken", log);
        broken->loading = test::RecordingScene::Load::Deferred;
        scenes.replace(broken, {.transition = SceneManager::Transition::fade(0.5F)});
        fixture.frames(2, 0.125);
        broken->deferral->fail(lua::Error("no map"));
        fixture.frames(1, 0.125);
        ASSERT_NE(fixture.engine().getError(), nullptr);
        EXPECT_STREQ(fixture.engine().getError()->what(), "no map");
    }
    {
        // The first scene has nothing to fall back to either.
        test::EngineFixture fixture;
        auto broken = std::make_shared<test::RecordingScene>("broken", log);
        broken->loading = test::RecordingScene::Load::Failing;
        fixture.engine().getScenes().push(broken);
        fixture.frames(1);
        ASSERT_NE(fixture.engine().getError(), nullptr);
        EXPECT_STREQ(fixture.engine().getError()->what(), "broken cannot load");
    }

    // An error handler takes the failure instead and routes the app to another scene, which comes after the reveal.
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    scenes.push(std::make_shared<test::RecordingScene>("menu", log));
    fixture.frames(1);
    auto broken = std::make_shared<test::RecordingScene>("broken", log);
    broken->loading = test::RecordingScene::Load::Deferred;
    auto fallback = std::make_shared<test::RecordingScene>("fallback", log);
    std::vector<std::string> errors;
    std::vector<SceneManager::Outcome> outcomes;
    // clang-format off
    scenes.replace(broken, {.transition = SceneManager::Transition::fade(0.5F), .completion = [&](const SceneManager::Result& result) { outcomes.push_back(result.outcome); }, .onError = [&](const lua::Error& error) {
        errors.emplace_back(error.what());
        scenes.replace(fallback);
    }});
    // clang-format on
    fixture.frames(2, 0.125);
    broken->deferral->fail(lua::Error("no map"));
    fixture.frames(1, 0.125);
    EXPECT_EQ(errors, std::vector<std::string>{"no map"});
    EXPECT_TRUE(scenes.empty());
    fixture.frames(2, 0.125);
    EXPECT_EQ(outcomes, std::vector<SceneManager::Outcome>{SceneManager::Outcome::Failed});
    EXPECT_EQ(scenes.getTop(), fallback.get());
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(SceneManagerTest, QueuesChangesInRequestOrder) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    scenes.push(std::make_shared<test::RecordingScene>("menu", log));
    fixture.frames(1);

    auto first = std::make_shared<test::RecordingScene>("first", log);
    first->loading = test::RecordingScene::Load::Deferred;
    auto second = std::make_shared<test::RecordingScene>("second", log);
    std::vector<std::string> order;
    scenes.replace(first, {.completion = [&](const SceneManager::Result&) { order.emplace_back("replace"); }});
    scenes.push(second, {.completion = [&](const SceneManager::Result&) { order.emplace_back("push"); }});
    scenes.pop({}, [&](const SceneManager::Result&) { order.emplace_back("pop"); });
    log.clear();
    fixture.frames(3);
    EXPECT_EQ(log, std::vector<std::string>{"first:load"});
    EXPECT_TRUE(order.empty());

    first->deferral->complete();
    fixture.frames(1);
    EXPECT_EQ(order, (std::vector<std::string>{"replace", "push", "pop"}));
    EXPECT_EQ(log, (std::vector<std::string>{"first:load", "menu:exitTransitionStarted", "menu:exit", "menu:unload", "first:enter", "first:enterTransitionFinished", "second:load", "first:exitTransitionStarted", "first:pause", "second:enter", "second:enterTransitionFinished", "second:exitTransitionStarted", "second:exit", "second:unload", "first:resume", "first:enterTransitionFinished"}));
    EXPECT_EQ(scenes.getTop(), first.get());
}

TEST(SceneManagerTest, PreloadsScenesInTheBackground) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto menu = std::make_shared<test::RecordingScene>("menu", log);
    scenes.push(menu);
    fixture.frames(1);

    // The game loads while the menu runs, without any change.
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    std::vector<SceneManager::Outcome> preloads;
    scenes.preload(game, std::string("save 2"), [&](const SceneManager::Result& result) { preloads.push_back(result.outcome); });
    EXPECT_EQ(game->getState(), Scene::State::Loading);
    EXPECT_FALSE(scenes.isTransitioning());
    const int updates = menu->updates;
    fixture.frames(2);
    EXPECT_EQ(menu->updates, updates + 2);
    EXPECT_THROW(scenes.preload(game), std::invalid_argument);
    game->deferral->complete();
    fixture.frames(1);
    EXPECT_EQ(game->getState(), Scene::State::Loaded);
    EXPECT_EQ(preloads, std::vector<SceneManager::Outcome>{SceneManager::Outcome::Completed});

    // A change takes the loaded scene at once, with the params of its preload.
    log.clear();
    scenes.replace(game);
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{"menu:exitTransitionStarted", "menu:exit", "menu:unload", "game:enter", "game:enterTransitionFinished"}));
    EXPECT_EQ(std::any_cast<std::string>(game->enterParams), "save 2");

    // A change that takes a scene still loading waits only for the rest of its load.
    auto next = std::make_shared<test::RecordingScene>("next", log);
    next->loading = test::RecordingScene::Load::Deferred;
    scenes.preload(next);
    scenes.replace(next, {.transition = SceneManager::Transition::fade(0.5F)});
    fixture.frames(3, 0.125);
    EXPECT_EQ(next->getState(), Scene::State::Loading);
    EXPECT_EQ(std::count(log.begin(), log.end(), "next:load"), 1);
    next->deferral->complete();
    fixture.frames(1, 0.125);
    EXPECT_EQ(log.back(), "next:enter");

    // A cancelled preload stops the load, unloads the scene and drops its completion.
    auto spare = std::make_shared<test::RecordingScene>("spare", log);
    spare->loading = test::RecordingScene::Load::Deferred;
    scenes.preload(spare, {}, [&](const SceneManager::Result& result) { preloads.push_back(result.outcome); });
    scenes.cancelPreload(*spare);
    EXPECT_EQ(log.back(), "spare:unload");
    EXPECT_EQ(spare->getState(), Scene::State::Unloaded);
    EXPECT_EQ(preloads.back(), SceneManager::Outcome::Dropped);
    EXPECT_THROW(scenes.cancelPreload(*spare), std::invalid_argument);

    // A preloaded scene keeps the params of its preload.
    auto keyed = std::make_shared<test::RecordingScene>("keyed", log);
    scenes.preload(keyed);
    scenes.push(keyed, {.params = 3});
    fixture.frames(4, 0.125);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "A preloaded scene keeps the params of its preload.");
}

TEST(SceneManagerTest, UnloadsAFailedPreloadOnceWhenAListenerCancelsIt) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto broken = std::make_shared<test::RecordingScene>("broken", log);
    broken->loading = test::RecordingScene::Load::Deferred;
    scenes.preload(broken);
    const Connection listener = fixture.engine().getEvents().on(LifecycleEvent::kSceneLoadFailed, [&](EventBus::Event&) { scenes.cancelPreload(*broken); });

    broken->deferral->fail(lua::Error("broken cannot load"));
    fixture.frames(1);
    EXPECT_EQ(std::count(log.begin(), log.end(), "broken:unload"), 1);
    EXPECT_EQ(broken->getState(), Scene::State::Unloaded);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(SceneManagerTest, LoadsThroughThePauseAndHaltsWithTheApp) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    SceneManager& scenes = engine.getScenes();
    scenes.push(std::make_shared<test::RecordingScene>("menu", log));
    fixture.frames(1);

    // The game pause stops neither the transition nor the loading view, which run on real time.
    auto view = std::make_shared<RecordingView>(log);
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(game, {.transition = SceneManager::Transition::fade(0.5F), .loading = view});
    engine.setPaused(true);
    fixture.frames(2, 0.125);
    EXPECT_EQ(game->getState(), Scene::State::Loading);
    EXPECT_TRUE(scenes.isLoadingViewShown());

    // A halted app lets no time pass for the change, and its load settles once the app runs again.
    engine.handleEvent({.type = platform::Event::Type::Suspended});
    ASSERT_TRUE(engine.isHalted());
    game->deferral->complete();
    const int updates = view->updates;
    fixture.frames(3, 0.125);
    EXPECT_EQ(game->getState(), Scene::State::Loading);
    EXPECT_EQ(view->updates, updates);
    engine.handleEvent({.type = platform::Event::Type::Resumed});
    fixture.frames(3, 0.125);
    EXPECT_EQ(game->getState(), Scene::State::Entering);
    EXPECT_FALSE(scenes.isLoadingViewShown());
    fixture.frames(2, 0.125);
    EXPECT_EQ(game->getState(), Scene::State::Active);
    EXPECT_EQ(game->updates, 0);
}

TEST(SceneManagerTest, FocusLossHaltsTheCoverAndTheLoadingDelay) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    engine.setLifecycle({.pauseOnFocusLoss = true});
    SceneManager& scenes = engine.getScenes();
    scenes.push(std::make_shared<test::RecordingScene>("menu", log));
    fixture.frames(1);

    // The cover stands still while the app is halted, and the first frame back takes no time.
    auto effect = std::make_shared<test::RecordingEffect>(0.5F, 0.5F);
    scenes.replace(std::make_shared<test::RecordingScene>("game", log), {.transition = {.duration = 1.0F, .effect = effect}});
    fixture.frames(1, 0.25);
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    fixture.frames(4, 0.25);
    EXPECT_EQ(effect->progresses.back(), 0.25F);
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    fixture.frames(1, 0.25);
    EXPECT_EQ(effect->progresses.back(), 0.25F);
    fixture.frames(3, 0.25);
    EXPECT_FALSE(scenes.isTransitioning());

    // The delay of a loading view counts real time only while the app runs.
    auto view = std::make_shared<RecordingView>(log);
    auto slow = std::make_shared<test::RecordingScene>("slow", log);
    slow->loading = test::RecordingScene::Load::Deferred;
    scenes.replace(slow, {.transition = {.duration = 0.5F, .effect = std::make_shared<test::RecordingEffect>(0.0F, 1.0F)}, .loading = view, .loadingDelay = 0.3F});
    fixture.frames(1, 0.2);
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    fixture.frames(5, 0.2);
    EXPECT_FALSE(scenes.isLoadingViewShown());
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    fixture.frames(2, 0.2);
    EXPECT_TRUE(scenes.isLoadingViewShown());
}

TEST(SceneManagerTest, StoppingTheEngineDuringALoadUnloadsEveryScene) {
    std::vector<std::string> log;
    auto game = std::make_shared<test::RecordingScene>("game", log);
    game->loading = test::RecordingScene::Load::Deferred;
    auto spare = std::make_shared<test::RecordingScene>("spare", log);
    spare->loading = test::RecordingScene::Load::Deferred;
    {
        test::EngineFixture fixture;
        SceneManager& scenes = fixture.engine().getScenes();
        scenes.push(std::make_shared<test::RecordingScene>("menu", log));
        fixture.frames(1);
        scenes.replace(game, {.transition = SceneManager::Transition::fade(0.5F), .loading = std::make_shared<RecordingView>(log)});
        scenes.preload(spare);
        fixture.frames(2, 0.125);
        log.clear();
    }
    EXPECT_EQ(log, (std::vector<std::string>{"view:exit", "spare:unload", "game:unload"}));
    EXPECT_EQ(game->getState(), Scene::State::Unloaded);

    // A deferral that finishes after the engine is gone changes nothing.
    game->deferral->complete();
    spare->deferral->fail(lua::Error("too late"));
    EXPECT_EQ(game->getState(), Scene::State::Unloaded);
}

TEST(SceneManagerTest, ClearingDropsPendingChangesAndEndsConnections) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    SceneManager& scenes = engine.getScenes();
    auto scene = std::make_shared<test::RecordingScene>("scene", log);
    scenes.push(scene);
    fixture.frames(1);

    Signal<> signal;
    int calls = 0;
    const Connection listened = scene->listen(signal, [&] { ++calls; });
    std::vector<std::string> events;
    for (const std::string_view name : {LifecycleEvent::kSceneExited, LifecycleEvent::kSceneUnloaded, LifecycleEvent::kSceneEntered}) {
        engine.getEvents().on(name, [&events, name](EventBus::Event& event) { events.push_back(std::string(name) + (event.get<Scene>() != nullptr ? " scene" : "")); });
    }
    std::vector<SceneManager::Outcome> outcomes;
    scenes.push(std::make_shared<test::RecordingScene>("dropped", log), {.transition = SceneManager::Transition::fade(1.0F), .completion = [&](const SceneManager::Result& result) { outcomes.push_back(result.outcome); }});
    scenes.pop({}, [&](const SceneManager::Result& result) { outcomes.push_back(result.outcome); });
    signal.emit();
    scenes.clear();
    signal.emit();
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(listened.isConnected());
    EXPECT_EQ(outcomes, (std::vector<SceneManager::Outcome>{SceneManager::Outcome::Dropped, SceneManager::Outcome::Dropped}));
    EXPECT_EQ(events, (std::vector<std::string>{"sceneExited scene", "sceneUnloaded scene"}));
}

TEST(SceneManagerTest, ProcessModesInheritDownTheStack) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    SceneManager& scenes = engine.getScenes();
    auto game = std::make_shared<test::RecordingScene>("game", log);
    auto menu = std::make_shared<test::RecordingScene>("menu", log, true, ProcessMode::WhenPaused);
    auto settings = std::make_shared<test::RecordingScene>("settings", log);
    scenes.push(game);
    scenes.push(menu);
    scenes.push(settings);
    fixture.frames(1);
    EXPECT_EQ(scenes.getProcessMode(0), ProcessMode::Pausable);
    EXPECT_EQ(scenes.getProcessMode(1), ProcessMode::WhenPaused);
    EXPECT_EQ(scenes.getProcessMode(2), ProcessMode::WhenPaused);

    // The settings scene inherits the mode of the menu, so it only runs and takes input while the game is paused.
    const int before = settings->updates;
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::A});
    engine.handleEvent({.type = platform::Event::Type::Resized});
    EXPECT_EQ(settings->updates, before);
    EXPECT_EQ(log.back(), "settings:event");
    log.clear();
    engine.setPaused(true);
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::B});
    EXPECT_EQ(settings->updates, before + 1);
    EXPECT_EQ(log, (std::vector<std::string>{"game:paused", "menu:unpaused", "settings:unpaused", "settings:event"}));

    // Scenes that run in both pause states, or in neither, hear nothing when the pause changes.
    scenes.push(std::make_shared<test::RecordingScene>("overlay", log, true, ProcessMode::Always));
    scenes.push(std::make_shared<test::RecordingScene>("frozen", log, true, ProcessMode::Disabled));
    fixture.frames(1);
    log.clear();
    engine.setPaused(false);
    EXPECT_EQ(log, (std::vector<std::string>{"game:unpaused", "menu:paused", "settings:paused"}));
}

TEST(SceneManagerTest, ChangesHoldInputBack) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    engine.getActions().load(Json::parse(R"({"actions": [{"name": "jump", "type": "button", "bindings": ["key:space"]}]})"));
    engine.getScenes().push(std::make_shared<test::RecordingScene>("scene", log));
    fixture.frames(1);

    SceneManager::Transition transition = SceneManager::Transition::fade(1.0F);
    engine.getScenes().push(std::make_shared<test::RecordingScene>("next", log), {.transition = transition});
    engine.handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::Space});
    fixture.frames(1);
    EXPECT_FALSE(engine.getActions().isDown("jump"));
    EXPECT_EQ(std::count(log.begin(), log.end(), "scene:event"), 0);

    // A key still held when the change ends counts as pressed only after it is released and pressed again.
    fixture.frames(60);
    ASSERT_FALSE(engine.getScenes().isInputBlocked());
    fixture.frames(1);
    EXPECT_FALSE(engine.getActions().isDown("jump"));
    engine.handleEvent({.type = platform::Event::Type::KeyUp, .key = input::Key::Space});
    fixture.frames(1);

    transition.blockInput = false;
    engine.getScenes().pop(transition);
    engine.handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::Space});
    fixture.frames(1);
    EXPECT_TRUE(engine.getActions().isPressed("jump"));
}

TEST(SceneManagerTest, EasesTransitionsOnRealTime) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    Engine& engine = fixture.engine();
    engine.getClock().setTimeScale(0.0);
    engine.setPaused(true);
    SceneManager::Transition transition = SceneManager::Transition::fade(1.0F);
    transition.ease = math::EasingCurve(math::Easing::Type::QuadIn);
    engine.getScenes().push(std::make_shared<test::RecordingScene>("menu", log), {.transition = transition});

    // With quad in, the eased progress reaches the halfway switch at about 0.71 of the time.
    fixture.frames(2, 0.25);
    fixture.frames(1, 0.2);
    EXPECT_EQ(log, std::vector<std::string>{"menu:load"});
    fixture.frames(1, 0.05);
    EXPECT_EQ(log, (std::vector<std::string>{"menu:load", "menu:enter"}));
    fixture.frames(2, 0.25);
    EXPECT_FALSE(engine.getScenes().isTransitioning());
}

TEST(SceneManagerTest, CustomEffectsChooseWhereTheyCoverTheScreen) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    auto effect = std::make_shared<test::RecordingEffect>(0.25F, 0.25F);
    fixture.engine().getScenes().push(std::make_shared<test::RecordingScene>("level", log), {.transition = {.duration = 1.0F, .effect = effect}});

    fixture.frames(1, 0.2);
    EXPECT_EQ(log, std::vector<std::string>{"level:load"});
    fixture.frames(1, 0.2);
    EXPECT_EQ(log.back(), "level:enter");
    fixture.frames(4, 0.2);
    EXPECT_EQ(log.back(), "level:enterTransitionFinished");
    ASSERT_EQ(effect->progresses.size(), 4U);
    EXPECT_NEAR(effect->progresses.front(), 0.2F, 1e-5F);
    EXPECT_NEAR(effect->progresses.back(), 0.8F, 1e-5F);
    EXPECT_THROW(fixture.engine().getScenes().push(std::make_shared<test::RecordingScene>("other", log), {.transition = {.duration = 1.0F}}), std::invalid_argument);
    EXPECT_THROW(fixture.engine().getScenes().push(std::make_shared<test::RecordingScene>("other", log), {.loadingDelay = -1.0F}), std::invalid_argument);
}

TEST(SceneManagerTest, PopsToLevelsAndListsTheStack) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto root = std::make_shared<test::RecordingScene>("root", log);
    scenes.push(root);
    scenes.push(std::make_shared<test::RecordingScene>("a", log));
    scenes.push(std::make_shared<test::RecordingScene>("b", log));
    scenes.push(std::make_shared<test::RecordingScene>("c", log));
    fixture.frames(1);
    EXPECT_EQ(scenes.size(), 4U);
    EXPECT_EQ(&scenes.at(0), root.get());
    EXPECT_EQ(scenes.find(*root), 0U);
    EXPECT_THROW((void)scenes.at(4), std::out_of_range);
    std::size_t visited = 0;
    for (const std::shared_ptr<Scene>& scene : scenes) {
        visited += scene ? 1U : 0U;
    }
    EXPECT_EQ(visited, 4U);

    log.clear();
    scenes.popTo(2);
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{"c:exitTransitionStarted", "c:exit", "c:unload", "b:exit", "b:unload", "a:resume", "a:enterTransitionFinished"}));

    // A pop that changes nothing ends without hooks.
    log.clear();
    std::vector<SceneManager::Outcome> outcomes;
    scenes.popTo(5, {}, [&](const SceneManager::Result& result) { outcomes.push_back(result.outcome); });
    scenes.popToRoot();
    fixture.frames(1);
    EXPECT_EQ(scenes.size(), 1U);
    EXPECT_EQ(outcomes, std::vector<SceneManager::Outcome>{SceneManager::Outcome::Completed});
    EXPECT_EQ(log, (std::vector<std::string>{"a:exitTransitionStarted", "a:exit", "a:unload", "root:resume", "root:enterTransitionFinished"}));
    EXPECT_FALSE(scenes.find(*std::make_shared<test::RecordingScene>("other", log)).has_value());

    // A scene can be on the stack only once.
    scenes.push(root);
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "The scene is already on the stack.");
}

TEST(SceneManagerTest, PopsWithLeavingScenesAndClearsThem) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    SceneManager& scenes = fixture.engine().getScenes();
    auto base = std::make_shared<test::RecordingScene>("base", log);
    auto overlay = std::make_shared<test::RecordingScene>("overlay", log, true);
    scenes.push(base);
    scenes.push(overlay);
    fixture.frames(1);
    const int rendered = base->renders;

    // The base shows in both images while the overlay leaves on top of it in the outgoing one.
    scenes.pop({.duration = 1.0F, .effect = std::make_shared<test::RecordingEffect>(0.0F, 1.0F)});
    log.clear();
    fixture.frames(1, 0.25);
    EXPECT_EQ(log, (std::vector<std::string>{"overlay:exitTransitionStarted", "base:resume"}));
    EXPECT_EQ(base->renders, rendered + 2);
    EXPECT_EQ(overlay->renders, rendered + 1);

    scenes.clear();
    EXPECT_EQ(log, (std::vector<std::string>{"overlay:exitTransitionStarted", "base:resume", "base:exit", "base:unload", "overlay:exit", "overlay:unload"}));
    fixture.frames(1);
    EXPECT_EQ(overlay->renders, rendered + 1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().passes, 1U);
}

} // namespace haylen::core
