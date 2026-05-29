#include "App.hpp"
#include "AppDetail.hpp"
#include "Util/BGM.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/SFX.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include "ConvertSketch.hpp"
#include "Enemy.hpp"
#include "AssetPaths.hpp"
#include "config.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

using namespace AppDetail;

void App::ResetGameSession() {
    m_Score = 0;
    m_Coins = 0;
    m_Lives = STARTING_LIVES;
    m_World = 1;
    m_Level = 1;
    m_StatusMessageTimer = 0.0f;
    m_StatusMessageAction = StatusMessageAction::None;
    m_DeathWasTimeout = false;
    m_TitleBlinkTimer = 0.0f;
    m_LowTimeWarningPlayed = false;
    m_GoalCelebrationPlayed = false;
    m_TransitionBlackoutTimer = 0.0f;
    m_TransitionPipeReached = false;
    m_TransitionPipeSoundPlayed = false;
    m_TransitionMarioHidden = false;
    m_TransitionAutoWalkStarted = false;
    m_TransitionDestination = TransitionDestination::LevelOneTwo;
    m_TransitionExitTimer = 0.0f;
    m_TransitionPipeEntryX = 0.0f;
    m_TransitionPipeEntryY = 0.0f;
    m_TransitionPipeSinkDistance = 0.0f;
    m_LevelIntroPurpose = LevelIntroPurpose::StartLevel;
}

bool App::LoadSceneSketch(const std::string& sketchPath, bool preserveProgress) {
    const int savedScore = m_Score;
    const int savedCoins = m_Coins;
    const int savedLives = m_Lives;
    const float savedLevelTimer = m_LevelTimer;
    const int savedDisplayedLevelTime = m_DisplayedLevelTime;
    const bool preserveMarioState = preserveProgress && m_Mario != nullptr;
    const Mario::PowerState savedPowerState =
        preserveMarioState ? m_Mario->GetPowerState() : Mario::PowerState::Small;

    g_MapManager = std::make_unique<MapManager>();
    m_Mario = std::make_unique<Mario>();
    m_Enemies.clear();
    m_PendingEnemySpawns.clear();
    m_Fireballs.clear();
    m_Pickups.clear();
    m_Debris.clear();
    m_FloatingTexts.clear();
    m_FireballCooldown = 0.0f;
    m_GoalSequenceStage = GoalSequenceStage::None;
    m_GoalSequenceTimer = 0.0f;
    m_CastleDoorX = 0.0f;
    m_GoalFlagMarioOffsetY = 0.0f;
    m_LevelIntroTimer = 0.0f;
    m_StatusMessageTimer = 0.0f;
    m_StatusMessageAction = StatusMessageAction::None;
    m_WasMarioDead = false;
    m_DeathWasTimeout = false;
    m_LowTimeWarningPlayed = false;
    m_GoalCelebrationPlayed = false;
    m_TransitionBlackoutTimer = 0.0f;
    m_TransitionPipeReached = false;
    m_TransitionPipeSoundPlayed = false;
    m_TransitionMarioHidden = false;
    m_TransitionAutoWalkStarted = false;
    m_TransitionDestination = TransitionDestination::LevelOneTwo;
    m_TransitionExitTimer = 0.0f;
    m_TransitionPipeEntryX = 0.0f;
    m_TransitionPipeEntryY = 0.0f;
    m_TransitionPipeSinkDistance = 0.0f;
    m_SkyColor = Util::Color(
        static_cast<unsigned char>(SKY_BLUE_R),
        static_cast<unsigned char>(SKY_BLUE_G),
        static_cast<unsigned char>(SKY_BLUE_B),
        255
    );
    m_CastleObject = nullptr;
    m_CastleImage = nullptr;

    if (preserveProgress) {
        m_Score = savedScore;
        m_Coins = savedCoins;
        m_Lives = savedLives;
        m_LevelTimer = savedLevelTimer;
        m_DisplayedLevelTime = savedDisplayedLevelTime;
    } else {
        m_Score = 0;
        m_Coins = 0;
        m_LevelTimer = STARTING_TIMER;
        m_DisplayedLevelTime = STARTING_TIMER;
    }

    std::vector<EnemySpawnInfo> enemySpawns;
    bool foundSpawn = convert_sketch(
        sketchPath,
        *g_MapManager,
        *m_Mario,
        m_SkyColor,
        &enemySpawns
    );

    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);

    for (const auto& spawn : enemySpawns) {
        m_PendingEnemySpawns.push_back({ spawn.position, spawn.kind, false });
    }

    if (!foundSpawn) {
        m_Mario->m_Transform.translation = { 0.0f, -200.0f };
    }
    if (preserveMarioState) {
        m_Mario->RestorePowerState(savedPowerState);
    }
    m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);

    m_ViewX = 0.0f;
    InitializeUi();
    return foundSpawn;
}

void App::LoadLevel(bool preserveProgress) {
    m_World = 1;
    m_Level = 1;
    LoadSceneSketch(AssetPaths::Image("LevelSketch0.png"), preserveProgress);
    m_CastleObject = std::make_shared<Util::GameObject>();
    m_CastleImage = std::make_shared<Util::Image>(AssetPaths::Image("castle1.png"));
    m_CastleObject->SetDrawable(m_CastleImage);
    m_CastleObject->SetZIndex(5.0f);
    if (g_MapManager && g_MapManager->HasGoal() && m_CastleImage != nullptr) {
        const float targetHeight = g_MapManager->GetTileSize() * CASTLE_TARGET_HEIGHT_TILES;
        const glm::vec2 castleSize = m_CastleImage->GetSize();
        float castleScale = 1.0f;
        float castleWidth = targetHeight;
        if (castleSize.y > 0.0f) {
            castleScale = targetHeight / castleSize.y;
            castleWidth = castleSize.x * castleScale;
        }

        const float minCastleCenter = g_MapManager->GetWorldLeft() + castleWidth * 0.5f;
        const float maxCastleCenter = g_MapManager->GetWorldRight()
            - castleWidth * 0.5f
            - g_MapManager->GetTileSize() * CASTLE_END_INSET_TILES;
        const float desiredCastleX = g_MapManager->GetGoalX() + g_MapManager->GetTileSize() * CASTLE_OFFSET_TILES;
        const float castleX = std::clamp(desiredCastleX, minCastleCenter, maxCastleCenter);
        const float castleGroundY = g_MapManager->GetGoalGroundY() - g_MapManager->GetTileSize();
        const float castleY = castleGroundY + targetHeight * 0.5f;

        m_CastleObject->m_Transform.translation = { castleX, castleY };
        m_CastleObject->m_Transform.scale = { castleScale, castleScale };
        m_CastleDoorX = castleX;
    }
}

void App::LoadLevelOneTwo(bool preserveMarioState) {
    m_World = 1;
    m_Level = 2;
    LoadSceneSketch(ResolveLevelOneTwoSketchPath(), true);
    if (!preserveMarioState && m_Mario) {
        m_Mario->RestorePowerState(Mario::PowerState::Small);
        m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);
    }
    m_LevelTimer = STARTING_TIMER;
    m_DisplayedLevelTime = STARTING_TIMER;
    RefreshHudText();
}

void App::ReloadCurrentLevel() {
    if (m_World == 1 && m_Level == 2) {
        LoadLevelOneTwo(false);
        return;
    }
    LoadLevel();
}

std::string App::ResolveTransitionSketchPath() const {
    const std::filesystem::path preferred = AssetPaths::ResourceRoot() / "image" / "transition1-1.png";
    if (std::filesystem::exists(preferred)) {
        return preferred.string();
    }

    const std::filesystem::path fallback = AssetPaths::ResourceRoot() / "image" / "transition1.png";
    if (std::filesystem::exists(fallback)) {
        return fallback.string();
    }

    return "";
}

std::string App::ResolveLevelOneTwoSketchPath() const {
    return AssetPaths::Image("LevelSketch1.png");
}

void App::BeginPostGoalTransition() {
    StopMusic();
    m_World = 1;
    m_Level = 2;
    m_LevelIntroTimer = POST_GOAL_INTRO_DURATION;
    m_LevelIntroPurpose = LevelIntroPurpose::PostGoalTransition;
    m_ScreenState = ScreenState::LevelIntro;
    RefreshHudText();
}

void App::LoadTransitionScene() {
    const std::string transitionSketch = ResolveTransitionSketchPath();
    if (transitionSketch.empty()) {
        m_CurrentState = State::END;
        return;
    }

    LoadSceneSketch(transitionSketch, true);
    StopMusic();
    m_ScreenState = ScreenState::TransitionScene;

    if (!m_Mario || !g_MapManager) {
        m_CurrentState = State::END;
        return;
    }

    const float tileSize = g_MapManager->GetTileSize();
    if (g_MapManager->HasTransitionPipe()) {
        m_TransitionPipeEntryX = g_MapManager->GetTransitionPipeEntryX();
    } else {
        m_TransitionPipeEntryX = std::clamp(
            g_MapManager->GetWorldRight() - tileSize * 2.0f,
            m_Mario->m_Transform.translation.x + tileSize * 0.5f,
            g_MapManager->GetWorldRight() - tileSize * 0.5f
        );
    }
    m_TransitionPipeEntryY = m_Mario->m_Transform.translation.y;
    m_TransitionPipeSinkDistance = tileSize * 1.2f;
    m_TransitionExitTimer = TRANSITION_BLACKOUT_DURATION;
    m_TransitionBlackoutTimer = 0.0f;
    m_TransitionPipeReached = false;
    m_TransitionPipeSoundPlayed = false;
    m_TransitionMarioHidden = false;
    m_TransitionAutoWalkStarted = false;
    m_TransitionDestination = TransitionDestination::LevelOneTwo;
    m_Mario->StartGoalWalk(m_TransitionPipeEntryX);
    m_TransitionAutoWalkStarted = true;
    RefreshHudText();
}

void App::BeginUndergroundExitTransition() {
    if (!m_Mario || !g_MapManager || !g_MapManager->HasTransitionPipe()) return;

    StopMusic(250);
    m_ScreenState = ScreenState::TransitionScene;
    m_TransitionDestination = TransitionDestination::UpperWorld;
    m_TransitionPipeEntryX = g_MapManager->GetTransitionPipeEntryX();
    m_TransitionPipeEntryY = m_Mario->m_Transform.translation.y;
    m_TransitionPipeSinkDistance = g_MapManager->GetTileSize() * 1.2f;
    m_TransitionExitTimer = TRANSITION_BLACKOUT_DURATION;
    m_TransitionBlackoutTimer = 0.0f;
    m_TransitionPipeReached = false;
    m_TransitionPipeSoundPlayed = false;
    m_TransitionMarioHidden = false;
    m_TransitionAutoWalkStarted = true;
    m_Fireballs.clear();
    m_FireballCooldown = 0.0f;
    m_Mario->StartGoalWalk(m_TransitionPipeEntryX);
}

void App::HandleMarioDeath() {
    if (!m_Mario || !m_Mario->IsDeathSequenceFinished()) return;

    if (m_DeathWasTimeout) {
        BeginStatusMessage(
            "TIME UP",
            STATUS_MESSAGE_DURATION,
            (m_Lives > 0) ? StatusMessageAction::ReloadLevel : StatusMessageAction::ShowGameOver
        );
        return;
    }

    if (m_Lives > 0) {
        ReloadCurrentLevel();
        StartLevelIntro(LEVEL_INTRO_DURATION);
        return;
    }

    PlaySfx(m_Audio.gameOver.get());
    BeginStatusMessage("GAME OVER", STATUS_MESSAGE_DURATION, StatusMessageAction::RestartToTitle);
}

