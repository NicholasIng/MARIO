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
    m_TransitionPipeMotion = TransitionPipeMotion::VerticalDown;
    m_TransitionExitTimer = 0.0f;
    m_TransitionPipeEntryX = 0.0f;
    m_TransitionPipeEntryY = 0.0f;
    m_TransitionPipeSinkDistance = 0.0f;
    m_TransitionPipeVisibleDistance = 0.0f;
    m_LevelIntroPurpose = LevelIntroPurpose::StartLevel;
}

bool App::LoadSceneSketch(const std::string& sketchPath, bool preserveProgress) {
    const std::string loweredSketchPath = [&]() {
        std::string value = sketchPath;
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }();
    const bool shouldKeepMarkerSpawn =
        loweredSketchPath.find("levelsketch1.png") != std::string::npos;

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
    m_TransitionPipeMotion = TransitionPipeMotion::VerticalDown;
    m_TransitionExitTimer = 0.0f;
    m_TransitionPipeEntryX = 0.0f;
    m_TransitionPipeEntryY = 0.0f;
    m_TransitionPipeSinkDistance = 0.0f;
    m_TransitionPipeVisibleDistance = 0.0f;
    m_SkyColor = Util::Color(
        static_cast<unsigned char>(SKY_BLUE_R),
        static_cast<unsigned char>(SKY_BLUE_G),
        static_cast<unsigned char>(SKY_BLUE_B),
        255
    );
    m_CastleObject = nullptr;
    m_CastleImage = nullptr;
    m_StartCastleObject = nullptr;
    m_StartCastleImage = nullptr;

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
        m_PendingEnemySpawns.push_back({
            spawn.position,
            spawn.kind,
            spawn.flightTopTiles,
            spawn.flightBottomTiles,
            false
        });
    }

    if (!foundSpawn) {
        m_Mario->m_Transform.translation = { 0.0f, -200.0f };
    }
    if (preserveMarioState) {
        m_Mario->RestorePowerState(savedPowerState);
    }
    if (!shouldKeepMarkerSpawn) {
        AlignMarioSpawnToGround();
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

void App::LoadLevelOneTwo(bool preserveMarioState, bool fallFromAbove) {
    m_World = 1;
    m_Level = 2;
    LoadSceneSketch(ResolveLevelOneTwoSketchPath(), true);
    if (m_Mario) {
        m_Mario->SetVisible(true);
    }
    if (!preserveMarioState && m_Mario) {
        m_Mario->RestorePowerState(Mario::PowerState::Small);
        AlignMarioSpawnToGround();
        m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);
    }
    if (fallFromAbove && m_Mario && g_MapManager) {
        m_Mario->m_Transform.translation.y += g_MapManager->GetTileSize() * 7.0f;
    }
    m_LevelTimer = STARTING_TIMER;
    m_DisplayedLevelTime = STARTING_TIMER;
    RefreshHudText();
}

void App::LoadLevelOneTwoExitArea(bool preserveMarioState) {
    m_World = 1;
    m_Level = 2;
    const std::filesystem::path exitSketch = AssetPaths::ResourceRoot() / "image" / "LevelSketch1-1.png";
    LoadSceneSketch(
        std::filesystem::exists(exitSketch) ? exitSketch.string() : AssetPaths::Image("OutdoorExitSketch.png"),
        true
    );
    if (m_Mario && g_MapManager && g_MapManager->HasTransitionPipe()) {
        m_Mario->m_Transform.translation.x = g_MapManager->GetTransitionPipeEntryX();
        m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);
    }
    if (!preserveMarioState && m_Mario) {
        m_Mario->RestorePowerState(Mario::PowerState::Small);
        AlignMarioSpawnToGround();
        m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);
    }
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
    RefreshHudText();
}

void App::LoadLevelOneThree(bool preserveMarioState) {
    m_World = 1;
    m_Level = 3;
    LoadSceneSketch(ResolveLevelOneThreeSketchPath(), true);
    PlaceGoalCastle("Castle.png");
    PlaceLevelStartCastleAndSpawn();
    if (!preserveMarioState && m_Mario) {
        m_Mario->RestorePowerState(Mario::PowerState::Small);
        m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);
    }
    m_LevelTimer = STARTING_TIMER;
    m_DisplayedLevelTime = STARTING_TIMER;
    RefreshHudText();
}

void App::PlaceGoalCastle(const std::string& imageName) {
    m_CastleObject = std::make_shared<Util::GameObject>();
    m_CastleImage = std::make_shared<Util::Image>(AssetPaths::Image(imageName));
    m_CastleObject->SetDrawable(m_CastleImage);
    m_CastleObject->SetZIndex(5.0f);

    if (!g_MapManager || !m_CastleImage) {
        return;
    }

    const glm::vec2 castleSize = m_CastleImage->GetSize();
    float castleScale = 3.0f;
    float castleWidth = castleSize.x > 0.0f ? castleSize.x : g_MapManager->GetTileSize() * CASTLE_TARGET_HEIGHT_TILES;
    float castleHeight = castleSize.y > 0.0f ? castleSize.y : g_MapManager->GetTileSize() * CASTLE_TARGET_HEIGHT_TILES;
    castleWidth *= castleScale;
    castleHeight *= castleScale;

    const float minCastleCenter = g_MapManager->GetWorldLeft() + castleWidth * 0.5f;
    const float maxCastleCenter = g_MapManager->GetWorldRight()
        - castleWidth * 0.5f
        - g_MapManager->GetTileSize() * CASTLE_END_INSET_TILES;
    const float desiredCastleX = g_MapManager->HasGoal()
        ? g_MapManager->GetGoalX() + g_MapManager->GetTileSize() * CASTLE_OFFSET_TILES
        : g_MapManager->GetWorldRight() - castleWidth * 0.5f - g_MapManager->GetTileSize();
    const float castleX = std::clamp(desiredCastleX, minCastleCenter, maxCastleCenter);
    const float castleGroundY = g_MapManager->HasGoal()
        ? g_MapManager->GetGoalGroundY() - g_MapManager->GetTileSize()
        : -(g_MapManager->GetHeight() * g_MapManager->GetTileSize()) / 2.0f;
    const float castleY = castleGroundY + castleHeight * 0.5f;

    m_CastleDoorX = castleX;
    m_CastleObject->m_Transform.translation = { castleX, castleY };
    m_CastleObject->m_Transform.scale = { castleScale, castleScale };
}

void App::PlaceLevelStartCastleAndSpawn() {
    if (!g_MapManager || !m_Mario) {
        return;
    }

    m_StartCastleObject = std::make_shared<Util::GameObject>();
    m_StartCastleImage = std::make_shared<Util::Image>(AssetPaths::Image("castle1.png"));
    m_StartCastleObject->SetDrawable(m_StartCastleImage);
    m_StartCastleObject->SetZIndex(5.0f);

    const float tileSize = g_MapManager->GetTileSize();
    const float targetHeight = tileSize * CASTLE_TARGET_HEIGHT_TILES;
    const glm::vec2 castleSize = m_StartCastleImage->GetSize();
    float castleScale = 1.0f;
    float castleWidth = targetHeight;
    if (castleSize.y > 0.0f) {
        castleScale = targetHeight / castleSize.y;
        castleWidth = castleSize.x * castleScale;
    }

    const float castleX = g_MapManager->GetWorldLeft() + castleWidth * 0.5f + tileSize;
    const float spawnX = castleX;
    const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
    const int gridX = std::clamp(
        static_cast<int>(std::floor((spawnX - g_MapManager->GetWorldLeft()) / tileSize)),
        0,
        std::max(0, g_MapManager->GetWidth() - 1)
    );

    float surfaceTop = -(g_MapManager->GetHeight() * tileSize) / 2.0f;
    for (int gridY = 0; gridY < g_MapManager->GetHeight(); ++gridY) {
        if (!MapManager::IsSolidCell(g_MapManager->GetCell(gridX, gridY))) {
            continue;
        }
        surfaceTop = mapTop - gridY * tileSize;
        break;
    }

    m_StartCastleObject->m_Transform.translation = {
        castleX,
        surfaceTop + targetHeight * 0.5f
    };
    m_StartCastleObject->m_Transform.scale = { castleScale, castleScale };

    m_Mario->m_Transform.translation = {
        spawnX,
        surfaceTop + m_Mario->GetHalfExtents().y
    };
    m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);
}

void App::AlignMarioSpawnToGround() {
    if (!m_Mario || !g_MapManager) return;

    const float tileSize = g_MapManager->GetTileSize();
    const int mapWidth = g_MapManager->GetWidth();
    const int mapHeight = g_MapManager->GetHeight();
    if (mapWidth <= 0 || mapHeight <= 0 || tileSize <= 0.0f) return;

    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapTop = (mapHeight * tileSize) / 2.0f;
    const glm::vec2 half = m_Mario->GetHalfExtents();
    const float x = m_Mario->m_Transform.translation.x;
    const float y = m_Mario->m_Transform.translation.y;
    const int centerGridX = std::clamp(
        static_cast<int>(std::floor((x - mapLeft) / tileSize)),
        0,
        mapWidth - 1
    );
    const int markerGridY = std::clamp(
        static_cast<int>(std::floor((mapTop - y) / tileSize)),
        0,
        mapHeight - 1
    );

    const int minSupportX = std::max(0, centerGridX - 1);
    const int maxSupportX = std::min(mapWidth - 1, centerGridX + 1);
    for (int gridY = markerGridY; gridY < mapHeight; ++gridY) {
        for (int gridX = minSupportX; gridX <= maxSupportX; ++gridX) {
            if (!MapManager::IsSolidCell(g_MapManager->GetCell(gridX, gridY))) {
                continue;
            }

            const float tileTop = mapTop - gridY * tileSize;
            m_Mario->m_Transform.translation.y = tileTop + half.y;
            return;
        }
    }
}

void App::ReloadCurrentLevel() {
    if (m_World == 1 && m_Level == 2) {
        LoadLevelOneTwo(false);
        return;
    }
    if (m_World == 1 && m_Level == 3) {
        LoadLevelOneThree(false);
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

std::string App::ResolveLevelOneThreeSketchPath() const {
    return AssetPaths::Image("LevelSketch1-3.png");
}

void App::BeginPostGoalTransition() {
    if (m_World == 1 && m_Level == 2) {
        LoadLevelOneThree(true);
        StartLevelIntro(LEVEL_INTRO_DURATION);
        return;
    }

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
    m_TransitionPipeVisibleDistance = m_TransitionPipeSinkDistance;
    m_TransitionExitTimer = TRANSITION_BLACKOUT_DURATION;
    m_TransitionBlackoutTimer = 0.0f;
    m_TransitionPipeReached = false;
    m_TransitionPipeSoundPlayed = false;
    m_TransitionMarioHidden = false;
    m_TransitionAutoWalkStarted = false;
    m_TransitionDestination = TransitionDestination::LevelOneTwo;
    m_TransitionPipeMotion = TransitionPipeMotion::VerticalDown;
    m_Mario->StartGoalWalk(m_TransitionPipeEntryX);
    m_TransitionAutoWalkStarted = true;
    RefreshHudText();
}

void App::BeginUndergroundExitTransition() {
    if (!m_Mario || !g_MapManager || !g_MapManager->HasTransitionPipe()) return;

    StopMusic(250);
    m_ScreenState = ScreenState::TransitionScene;
    m_TransitionDestination = TransitionDestination::LevelOneTwoExitArea;
    m_TransitionPipeMotion = TransitionPipeMotion::HorizontalRight;
    m_TransitionPipeEntryX = m_Mario->m_Transform.translation.x;
    m_TransitionPipeEntryY = m_Mario->m_Transform.translation.y;
    m_TransitionPipeSinkDistance = g_MapManager->GetTileSize() * 2.0f;
    m_TransitionPipeVisibleDistance = g_MapManager->GetTileSize() * 0.8f;
    m_TransitionExitTimer = TRANSITION_BLACKOUT_DURATION;
    m_TransitionBlackoutTimer = 0.0f;
    m_TransitionPipeReached = false;
    m_TransitionPipeSoundPlayed = false;
    m_TransitionMarioHidden = false;
    m_TransitionAutoWalkStarted = false;
    m_Fireballs.clear();
    m_FireballCooldown = 0.0f;
    m_TransitionPipeReached = true;
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

