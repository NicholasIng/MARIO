#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include "ConvertSketch.hpp"
#include "Enemy.hpp"
#include "AssetPaths.hpp"
#include "config.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

// global pointer for Mario collision
std::unique_ptr<MapManager> g_MapManager;

namespace {

constexpr float SKY_BLUE_R = 90.0f;
constexpr float SKY_BLUE_G = 147.0f;
constexpr float SKY_BLUE_B = 235.0f;
constexpr float CASTLE_TARGET_HEIGHT_TILES = 6.0f;
constexpr float CASTLE_OFFSET_TILES = 7.0f;
constexpr float CASTLE_END_INSET_TILES = 1.0f;
constexpr float GOAL_FINISH_DELAY = 0.55f;
constexpr float ENEMY_SPAWN_RANGE_X = WINDOW_WIDTH * 0.75f;
constexpr float LEVEL_INTRO_DURATION = 1.75f;
constexpr int STARTING_TIMER = 400;
constexpr int STARTING_LIVES = 3;

std::string PadNumber(int value, int width) {
    std::ostringstream stream;
    stream << std::setw(width) << std::setfill('0') << std::max(0, value);
    return stream.str();
}

std::string WorldLabel(int world, int level) {
    return std::to_string(world) + "-" + std::to_string(level);
}

} // namespace

void App::ActivateNearbyEnemies() {
    if (!m_Mario) return;

    for (auto& pending : m_PendingEnemySpawns) {
        if (pending.activated) continue;
        if (std::abs(pending.position.x - m_Mario->m_Transform.translation.x) > ENEMY_SPAWN_RANGE_X) {
            continue;
        }

        m_Enemies.push_back(std::make_unique<Enemy>(pending.position.x, pending.position.y));
        pending.activated = true;
    }
}

void App::StartGoalSequence() {
    if (!m_Mario || !g_MapManager || m_GoalSequenceStage != GoalSequenceStage::None) return;

    m_GoalSequenceStage = GoalSequenceStage::Sliding;
    m_GoalSequenceTimer = 0.0f;
    m_GoalFlagMarioOffsetY = 0.0f;
    m_Fireballs.clear();
    m_FireballCooldown = 0.0f;
    AddScore(static_cast<int>(std::max(0.0f, m_LevelTimer)) * 10);

    if (m_CastleObject != nullptr && m_CastleImage != nullptr) {
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

        m_CastleDoorX = castleX;
        m_CastleObject->m_Transform.translation = { castleX, castleY };
        m_CastleObject->m_Transform.scale = { castleScale, castleScale };
    } else {
        m_CastleDoorX = g_MapManager->GetWorldRight() - g_MapManager->GetTileSize() * 2.0f;
    }

    const float touchY = std::clamp(
        m_Mario->m_Transform.translation.y,
        g_MapManager->GetFlagBottomY(),
        g_MapManager->GetFlagTopY()
    );
    g_MapManager->SetFlagY(touchY);

    m_Mario->StartGoalSequence(
        g_MapManager->GetGoalX(),
        g_MapManager->GetFlagX(),
        g_MapManager->GetFlagBottomY(),
        g_MapManager->GetGoalGroundY(),
        touchY
    );
    m_GoalFlagMarioOffsetY = touchY - m_Mario->m_Transform.translation.y;
}

void App::UpdateGoalSequence(float dt) {
    if (!m_Mario || !g_MapManager || m_GoalSequenceStage == GoalSequenceStage::None) return;

    if (m_GoalSequenceStage == GoalSequenceStage::Sliding) {
        m_Mario->Update();
        const float nextFlagY = m_Mario->m_Transform.translation.y + m_GoalFlagMarioOffsetY;
        g_MapManager->SetFlagY(nextFlagY);
        if (m_Mario->IsGoalSequenceFinished()) {
            g_MapManager->SetFlagY(g_MapManager->GetFlagBottomY());
            m_GoalSequenceStage = GoalSequenceStage::PlayerControl;
        }
    } else if (m_GoalSequenceStage == GoalSequenceStage::PlayerControl) {
        if (m_Mario->m_Transform.translation.x >= m_CastleDoorX) {
            m_Mario->m_Transform.translation.x = m_CastleDoorX;
            m_Mario->SetVisible(false);
            m_GoalSequenceStage = GoalSequenceStage::Entering;
            m_GoalSequenceTimer = GOAL_FINISH_DELAY;
        }
    } else if (m_GoalSequenceStage == GoalSequenceStage::Entering) {
        m_GoalSequenceTimer = std::max(0.0f, m_GoalSequenceTimer - dt);
        if (m_GoalSequenceTimer <= 0.0f) {
            m_GoalSequenceStage = GoalSequenceStage::Finished;
            m_CurrentState = State::END;
        }
    }
}

App::HudText App::CreateTextObject(const std::string& text,
                                   int size,
                                   const glm::vec2& position,
                                   const Util::Color& color,
                                   const glm::vec2& scale) {
    auto drawable = std::make_shared<Util::Text>(m_PixelFontPath, size, text, color);
    auto object = std::make_shared<Util::GameObject>();
    object->SetDrawable(drawable);
    object->m_Transform.translation = position;
    object->m_Transform.scale = scale;
    object->SetZIndex(1000.0f);
    return { drawable, object };
}

void App::DrawUiObject(const std::shared_ptr<Util::GameObject>& object) const {
    if (object != nullptr) {
        object->Draw();
    }
}

void App::InitializeUi() {
    namespace fs = std::filesystem;

    const fs::path root = AssetPaths::ResourceRoot().parent_path();
    m_FontPath = (root / "PTSD" / "assets" / "fonts" / "Inter.ttf").string();
    m_PixelFontPath = (root / "PTSD" / "lib" / "imgui" / "misc" / "fonts" / "ProggyClean.ttf").string();

    m_HudCoinIcon = std::make_shared<Util::GameObject>();
    m_HudCoinIcon->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("coin1.png")));
    m_HudCoinIcon->m_Transform.scale = { 2.0f, 2.0f };
    m_HudCoinIcon->SetZIndex(1000.0f);

    m_TitleMountain = std::make_shared<Util::GameObject>();
    m_TitleMountain->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("mountains.png")));
    m_TitleMountain->m_Transform.translation = { -470.0f, -255.0f };
    m_TitleMountain->m_Transform.scale = { 7.5f, 7.5f };
    m_TitleMountain->SetZIndex(900.0f);

    m_TitleBush = std::make_shared<Util::GameObject>();
    m_TitleBush->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Bush.png")));
    m_TitleBush->m_Transform.translation = { 420.0f, -250.0f };
    m_TitleBush->m_Transform.scale = { 6.0f, 6.0f };
    m_TitleBush->SetZIndex(910.0f);

    m_TitleMario = std::make_shared<Util::GameObject>();
    m_TitleMario->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Character/MarioIdle.png")));
    m_TitleMario->m_Transform.translation = { -355.0f, -265.0f };
    m_TitleMario->m_Transform.scale = { 4.5f, 4.5f };
    m_TitleMario->SetZIndex(920.0f);

    m_IntroMario = std::make_shared<Util::GameObject>();
    m_IntroMario->SetDrawable(std::make_shared<Util::Image>(AssetPaths::Image("Character/MarioIdle.png")));
    m_IntroMario->m_Transform.translation = { -85.0f, -20.0f };
    m_IntroMario->m_Transform.scale = { 5.0f, 5.0f };
    m_IntroMario->SetZIndex(1000.0f);

    const Util::Color white(255, 255, 255, 255);
    const Util::Color peach(255, 205, 190, 255);

    m_HudMarioLabel = CreateTextObject("MARIO", 28, { -520.0f, 328.0f }, white, { 2.2f, 2.2f });
    m_HudScoreValue = CreateTextObject("000000", 28, { -520.0f, 286.0f }, white, { 2.2f, 2.2f });
    m_HudWorldLabel = CreateTextObject("WORLD", 28, { 92.0f, 328.0f }, white, { 2.2f, 2.2f });
    m_HudWorldValue = CreateTextObject("1-1", 28, { 140.0f, 286.0f }, white, { 2.2f, 2.2f });
    m_HudTimeLabel = CreateTextObject("TIME", 28, { 415.0f, 328.0f }, white, { 2.2f, 2.2f });
    m_HudTimeValue = CreateTextObject("400", 28, { 445.0f, 286.0f }, white, { 2.2f, 2.2f });
    m_HudCoinValue = CreateTextObject("x00", 28, { -95.0f, 296.0f }, white, { 2.2f, 2.2f });

    m_TitleLogo = CreateTextObject("SUPER\nMARIO BROS.", 44, { -320.0f, 110.0f }, peach, { 3.2f, 3.2f });
    m_TitleCopyright = CreateTextObject("(C)1985 NINTENDO", 20, { -40.0f, -65.0f }, peach, { 2.6f, 2.6f });
    m_TitleOption1 = CreateTextObject("1 PLAYER GAME", 24, { -160.0f, -150.0f }, white, { 2.5f, 2.5f });
    m_TitleOption2 = CreateTextObject("2 PLAYER GAME", 24, { -160.0f, -210.0f }, white, { 2.5f, 2.5f });
    m_TitleTopScore = CreateTextObject("TOP- 000000", 22, { -105.0f, -305.0f }, white, { 2.4f, 2.4f });
    m_TitlePressStart = CreateTextObject("PRESS ENTER", 18, { -68.0f, -345.0f }, white, { 2.2f, 2.2f });

    m_IntroWorldText = CreateTextObject("WORLD  1-1", 28, { -120.0f, 110.0f }, white, { 2.8f, 2.8f });
    m_IntroLivesText = CreateTextObject("x 3", 28, { 35.0f, -18.0f }, white, { 2.8f, 2.8f });

    RefreshHudText();
}

void App::RefreshHudText() {
    if (m_HudScoreValue.drawable) {
        m_HudScoreValue.drawable->SetText(PadNumber(m_Score, 6));
    }
    if (m_HudWorldValue.drawable) {
        m_HudWorldValue.drawable->SetText(WorldLabel(m_World, m_Level));
    }
    if (m_HudTimeValue.drawable) {
        m_HudTimeValue.drawable->SetText(PadNumber(static_cast<int>(std::floor(std::max(0.0f, m_LevelTimer))), 3));
    }
    if (m_HudCoinValue.drawable) {
        m_HudCoinValue.drawable->SetText("x" + PadNumber(m_Coins, 2));
    }
    if (m_TitleTopScore.drawable) {
        m_TitleTopScore.drawable->SetText("TOP- " + PadNumber(m_TopScore, 6));
    }
    if (m_IntroWorldText.drawable) {
        m_IntroWorldText.drawable->SetText("WORLD  " + WorldLabel(m_World, m_Level));
    }
    if (m_IntroLivesText.drawable) {
        m_IntroLivesText.drawable->SetText("x " + std::to_string(std::max(0, m_Lives)));
    }
}

void App::AddScore(int points) {
    if (points <= 0) return;
    m_Score += points;
    m_TopScore = std::max(m_TopScore, m_Score);
    RefreshHudText();
}

void App::StartLevelIntro(float duration) {
    m_LevelIntroTimer = duration;
    m_ScreenState = ScreenState::LevelIntro;
    RefreshHudText();
}

void App::Start() {
    LOG_TRACE("Start");

    g_MapManager = std::make_unique<MapManager>();
    m_Mario = std::make_unique<Mario>();
    m_Enemies.clear();
    m_PendingEnemySpawns.clear();
    m_Fireballs.clear();
    m_Pickups.clear();
    m_Debris.clear();
    m_FireballCooldown = 0.0f;
    m_GoalSequenceStage = GoalSequenceStage::None;
    m_GoalSequenceTimer = 0.0f;
    m_CastleDoorX = 0.0f;
    m_GoalFlagMarioOffsetY = 0.0f;
    m_Score = 0;
    m_Coins = 0;
    m_Lives = STARTING_LIVES;
    m_World = 1;
    m_Level = 1;
    m_LevelTimer = STARTING_TIMER;
    m_WasMarioDead = false;
    m_SkyColor = Util::Color(
        static_cast<unsigned char>(SKY_BLUE_R),
        static_cast<unsigned char>(SKY_BLUE_G),
        static_cast<unsigned char>(SKY_BLUE_B),
        255
    );

    std::vector<glm::vec2> enemySpawns;
    bool foundSpawn = convert_sketch(
        AssetPaths::Image("LevelSketch0.png"),
        *g_MapManager,
        *m_Mario,
        m_SkyColor,
        &enemySpawns
    );

    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);

    for (const auto& spawn : enemySpawns) {
        m_PendingEnemySpawns.push_back({ spawn, false });
    }

    if (!foundSpawn) {
        m_Mario->m_Transform.translation = { 0.0f, -200.0f };
    }
    m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);

    m_ViewX = 0.0f;
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

    InitializeUi();

    LOG_TRACE("Map size = {} x {}", g_MapManager->GetWidth(), g_MapManager->GetHeight());
    LOG_TRACE("Mario start pos = {}, {}",
        m_Mario->m_Transform.translation.x,
        m_Mario->m_Transform.translation.y);

    m_ScreenState = ScreenState::Title;
    m_CurrentState = State::UPDATE;
}

void App::DrawHud() {
    m_HudCoinIcon->m_Transform.translation = { -150.0f, 304.0f };
    DrawUiObject(m_HudMarioLabel.object);
    DrawUiObject(m_HudScoreValue.object);
    DrawUiObject(m_HudWorldLabel.object);
    DrawUiObject(m_HudWorldValue.object);
    DrawUiObject(m_HudTimeLabel.object);
    DrawUiObject(m_HudTimeValue.object);
    DrawUiObject(m_HudCoinIcon);
    DrawUiObject(m_HudCoinValue.object);
}

void App::RenderTitleScreen() {
    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);
    m_HudCoinIcon->m_Transform.translation = { -150.0f, 304.0f };
    DrawUiObject(m_HudMarioLabel.object);
    DrawUiObject(m_HudScoreValue.object);
    DrawUiObject(m_HudWorldLabel.object);
    DrawUiObject(m_HudWorldValue.object);
    DrawUiObject(m_HudTimeLabel.object);
    DrawUiObject(m_HudCoinIcon);
    DrawUiObject(m_HudCoinValue.object);
    DrawUiObject(m_TitleMountain);
    DrawUiObject(m_TitleBush);
    DrawUiObject(m_TitleMario);
    DrawUiObject(m_TitleLogo.object);
    DrawUiObject(m_TitleCopyright.object);
    DrawUiObject(m_TitleOption1.object);
    DrawUiObject(m_TitleOption2.object);
    DrawUiObject(m_TitleTopScore.object);
    DrawUiObject(m_TitlePressStart.object);
}

void App::RenderLevelIntro() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_HudCoinIcon->m_Transform.translation = { -150.0f, 304.0f };
    DrawUiObject(m_HudMarioLabel.object);
    DrawUiObject(m_HudScoreValue.object);
    DrawUiObject(m_HudWorldLabel.object);
    DrawUiObject(m_HudWorldValue.object);
    DrawUiObject(m_HudTimeLabel.object);
    DrawUiObject(m_HudCoinIcon);
    DrawUiObject(m_HudCoinValue.object);
    DrawUiObject(m_IntroWorldText.object);
    DrawUiObject(m_IntroMario);
    DrawUiObject(m_IntroLivesText.object);
}

void App::RenderGameplay() {
    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);

    if (g_MapManager) {
        g_MapManager->Draw(m_ViewX);
    }

    if (m_CastleObject != nullptr &&
        g_MapManager != nullptr) {
        const glm::vec2 oldPos = m_CastleObject->m_Transform.translation;
        m_CastleObject->m_Transform.translation.x -= m_ViewX;
        m_CastleObject->Draw();
        m_CastleObject->m_Transform.translation = oldPos;
    }
    if (m_Mario) {
        const glm::vec2 oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= m_ViewX;
        m_Mario->m_Transform.translation.y += m_Mario->GetRenderOffsetY();
        m_Mario->Draw();
        m_Mario->m_Transform.translation = oldPos;
    }
    for (auto& enemy : m_Enemies) {
        const glm::vec2 oldPos = enemy->m_Transform.translation;
        enemy->m_Transform.translation.x -= m_ViewX;
        enemy->Draw();
        enemy->m_Transform.translation = oldPos;
    }
    for (auto& fireball : m_Fireballs) {
        const glm::vec2 oldPos = fireball->m_Transform.translation;
        fireball->m_Transform.translation.x -= m_ViewX;
        fireball->Draw();
        fireball->m_Transform.translation = oldPos;
    }
    for (auto& pickup : m_Pickups) {
        const glm::vec2 oldPos = pickup->m_Transform.translation;
        pickup->m_Transform.translation.x -= m_ViewX;
        pickup->Draw();
        pickup->m_Transform.translation = oldPos;
    }
    for (auto& debris : m_Debris) {
        const glm::vec2 oldPos = debris->m_Transform.translation;
        debris->m_Transform.translation.x -= m_ViewX;
        debris->Draw();
        debris->m_Transform.translation = oldPos;
    }

    DrawHud();
}

void App::UpdateTitleScreen(float) {
    if (Util::Input::IsKeyPressed(Util::Keycode::RETURN) ||
        Util::Input::IsKeyPressed(Util::Keycode::SPACE)) {
        StartLevelIntro(LEVEL_INTRO_DURATION);
    }

    RenderTitleScreen();
}

void App::UpdateLevelIntro(float dt) {
    m_LevelIntroTimer = std::max(0.0f, m_LevelIntroTimer - dt);
    if (m_LevelIntroTimer <= 0.0f) {
        m_ScreenState = ScreenState::Gameplay;
    }

    RenderLevelIntro();
}

void App::UpdateGameplay(float dt) {
    const bool powerupFreezeActive = m_Mario && m_Mario->IsTransforming();
    if (g_MapManager && !powerupFreezeActive) {
        g_MapManager->Update();
    }

    if (!powerupFreezeActive) {
        LootType lootType;
        glm::vec2 lootPos;
        while (g_MapManager && g_MapManager->PollSpawnEvent(lootType, lootPos)) {
            m_Pickups.push_back(std::make_unique<Pickup>(lootType, lootPos.x, lootPos.y));
        }

        glm::vec2 breakPos;
        while (g_MapManager && g_MapManager->PollBrickBreakEvent(breakPos)) {
            AddScore(50);
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::TopLeft));
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::TopRight));
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::BottomLeft));
            m_Debris.push_back(std::make_unique<Debris>(breakPos.x, breakPos.y, Debris::Piece::BottomRight));

            const float tileSize = g_MapManager->GetTileSize();
            const float brickTop = breakPos.y + tileSize * 0.5f;
            for (auto& enemy : m_Enemies) {
                if (!enemy->IsAlive()) continue;

                const glm::vec2 enemyHalf = enemy->GetHalfExtents();
                const float enemyBottom = enemy->m_Transform.translation.y - enemyHalf.y;
                const bool overlapsBrickX =
                    std::abs(enemy->m_Transform.translation.x - breakPos.x) <= (tileSize * 0.5f + enemyHalf.x - 2.0f);
                const bool standingOnBrick =
                    enemyBottom >= brickTop - 8.0f &&
                    enemyBottom <= brickTop + tileSize * 0.75f;
                if (overlapsBrickX && standingOnBrick) {
                    const float horizontalKnockback =
                        (enemy->m_Transform.translation.x >= breakPos.x) ? 80.0f : -80.0f;
                    enemy->KillFlipped(horizontalKnockback);
                    AddScore(100);
                }
            }
        }

        glm::vec2 coinPos;
        while (g_MapManager && g_MapManager->PollCoinCollectEvent(coinPos)) {
            ++m_Coins;
            AddScore(100);
        }
    }

    const bool goalSequenceActive = m_GoalSequenceStage != GoalSequenceStage::None;
    const bool manualGoalControl = m_GoalSequenceStage == GoalSequenceStage::PlayerControl;
    const bool autoGoalSequence = goalSequenceActive && !manualGoalControl;
    if (autoGoalSequence) {
        UpdateGoalSequence(dt);
    } else {
        if (m_Mario) {
            m_Mario->Update();
        }
        if (!powerupFreezeActive) {
            ActivateNearbyEnemies();
            for (auto& enemy : m_Enemies) {
                enemy->Update();
            }
            for (size_t i = 0; i < m_Enemies.size(); ++i) {
                Enemy* leftEnemy = m_Enemies[i].get();
                if (!leftEnemy->IsAlive()) continue;

                for (size_t j = i + 1; j < m_Enemies.size(); ++j) {
                    Enemy* rightEnemy = m_Enemies[j].get();
                    if (!rightEnemy->IsAlive()) continue;

                    const glm::vec2 leftHalf = leftEnemy->GetHalfExtents();
                    const glm::vec2 rightHalf = rightEnemy->GetHalfExtents();

                    const float leftLeft = leftEnemy->m_Transform.translation.x - leftHalf.x;
                    const float leftRight = leftEnemy->m_Transform.translation.x + leftHalf.x;
                    const float leftBottom = leftEnemy->m_Transform.translation.y - leftHalf.y;
                    const float leftTop = leftEnemy->m_Transform.translation.y + leftHalf.y;

                    const float rightLeft = rightEnemy->m_Transform.translation.x - rightHalf.x;
                    const float rightRight = rightEnemy->m_Transform.translation.x + rightHalf.x;
                    const float rightBottom = rightEnemy->m_Transform.translation.y - rightHalf.y;
                    const float rightTop = rightEnemy->m_Transform.translation.y + rightHalf.y;

                    const bool overlapX = leftRight > rightLeft && leftLeft < rightRight;
                    const bool overlapY = leftTop > rightBottom && leftBottom < rightTop;
                    if (!overlapX || !overlapY) continue;

                    const float overlapAmount =
                        std::min(leftRight, rightRight) - std::max(leftLeft, rightLeft);
                    if (overlapAmount <= 0.0f) continue;

                    const float separation = overlapAmount * 0.5f + 0.05f;
                    if (leftEnemy->m_Transform.translation.x <= rightEnemy->m_Transform.translation.x) {
                        leftEnemy->m_Transform.translation.x -= separation;
                        rightEnemy->m_Transform.translation.x += separation;
                    } else {
                        leftEnemy->m_Transform.translation.x += separation;
                        rightEnemy->m_Transform.translation.x -= separation;
                    }

                    leftEnemy->SetDirection(-leftEnemy->GetDirection());
                    rightEnemy->SetDirection(-rightEnemy->GetDirection());
                }
            }
            for (auto& fireball : m_Fireballs) {
                fireball->Update();
            }
            for (auto& pickup : m_Pickups) {
                pickup->Update();
            }
            for (auto& debris : m_Debris) {
                debris->Update();
            }
        }
    }

    if (!autoGoalSequence && !powerupFreezeActive && m_Mario && !m_Mario->IsDead()) {
        m_LevelTimer = std::max(0.0f, m_LevelTimer - dt);
        if (m_LevelTimer <= 0.0f) {
            m_Mario->Die();
        }
    }

    if (!m_WasMarioDead && m_Mario && m_Mario->IsDead()) {
        m_Lives = std::max(0, m_Lives - 1);
        RefreshHudText();
    }
    m_WasMarioDead = m_Mario && m_Mario->IsDead();

    if (!powerupFreezeActive) {
        m_FireballCooldown = std::max(0.0f, m_FireballCooldown - dt);
    }
    if (!autoGoalSequence && !powerupFreezeActive &&
        m_Mario && m_Mario->IsFire() && !m_Mario->IsDead() &&
        Util::Input::IsKeyDown(Util::Keycode::F) &&
        m_FireballCooldown <= 0.0f &&
        m_Fireballs.size() < 2) {
        const glm::vec2 spawn = m_Mario->GetFireballSpawnPosition();
        m_Fireballs.push_back(std::make_unique<Fireball>(
            spawn.x,
            spawn.y,
            m_Mario->GetFacingDirection()
        ));
        m_FireballCooldown = 0.18f;
    }

    if (!autoGoalSequence && !powerupFreezeActive && m_Mario && !m_Mario->IsDead()) {
        const glm::vec2 marioHalf = m_Mario->GetHalfExtents();
        for (auto& enemy : m_Enemies) {
            if (!enemy->IsAlive()) continue;

            const glm::vec2 enemyHalf = enemy->GetHalfExtents();
            const float marioLeft = m_Mario->m_Transform.translation.x - marioHalf.x;
            const float marioRight = m_Mario->m_Transform.translation.x + marioHalf.x;
            const float marioBottom = m_Mario->m_Transform.translation.y - marioHalf.y;
            const float marioTop = m_Mario->m_Transform.translation.y + marioHalf.y;

            const float enemyLeft = enemy->m_Transform.translation.x - enemyHalf.x;
            const float enemyRight = enemy->m_Transform.translation.x + enemyHalf.x;
            const float enemyBottom = enemy->m_Transform.translation.y - enemyHalf.y;
            const float enemyTop = enemy->m_Transform.translation.y + enemyHalf.y;

            const bool overlapX = marioRight > enemyLeft && marioLeft < enemyRight;
            const bool overlapY = marioTop > enemyBottom && marioBottom < enemyTop;
            if (!overlapX || !overlapY) continue;

            const float horizontalOverlap =
                std::min(marioRight, enemyRight) - std::max(marioLeft, enemyLeft);
            const float verticalOverlap =
                std::min(marioTop, enemyTop) - std::max(marioBottom, enemyBottom);
            const float stompForgiveness = 18.0f;
            const float minimumStompWidth = enemyHalf.x * 0.35f;
            const bool stomped =
                m_Mario->GetVelocityY() < -30.0f &&
                m_Mario->m_Transform.translation.y >= enemy->m_Transform.translation.y - 8.0f &&
                marioBottom >= enemyTop - stompForgiveness &&
                horizontalOverlap >= minimumStompWidth &&
                verticalOverlap <= enemyHalf.y + stompForgiveness;

            if (m_Mario->HasStarPower()) {
                enemy->KillFlipped(110.0f * m_Mario->GetFacingDirection());
                AddScore(100);
            } else if (stomped) {
                enemy->Stomp();
                m_Mario->BounceAfterStomp();
                AddScore(100);
            } else if (!m_Mario->IsInvulnerable()) {
                m_Mario->TakeEnemyHit();
                break;
            }
        }

        for (auto& pickup : m_Pickups) {
            if (pickup->IsCollected()) continue;
            const glm::vec2 pickupHalf = pickup->GetHalfExtents();
            const float pickupLeft = pickup->m_Transform.translation.x - pickupHalf.x;
            const float pickupRight = pickup->m_Transform.translation.x + pickupHalf.x;
            const float pickupBottom = pickup->m_Transform.translation.y - pickupHalf.y;
            const float pickupTop = pickup->m_Transform.translation.y + pickupHalf.y;
            const float marioLeft = m_Mario->m_Transform.translation.x - marioHalf.x;
            const float marioRight = m_Mario->m_Transform.translation.x + marioHalf.x;
            const float marioBottom = m_Mario->m_Transform.translation.y - marioHalf.y;
            const float marioTop = m_Mario->m_Transform.translation.y + marioHalf.y;
            const bool overlapX = marioRight > pickupLeft && marioLeft < pickupRight;
            const bool overlapY = marioTop > pickupBottom && marioBottom < pickupTop;
            if (overlapX && overlapY) {
                const LootType type = pickup->GetType();
                m_Mario->PowerUp(type);
                if (type == LootType::Coin) {
                    ++m_Coins;
                    AddScore(100);
                } else if (type == LootType::GreenMushroom) {
                    ++m_Lives;
                    AddScore(1000);
                } else {
                    AddScore(1000);
                }
                pickup->Collect();
                RefreshHudText();
            }
        }

        for (auto& fireball : m_Fireballs) {
            if (fireball->IsExpired() || fireball->IsExploding()) continue;

            const glm::vec2 fireballHalf = fireball->GetHalfExtents();
            const float fireballLeft = fireball->m_Transform.translation.x - fireballHalf.x;
            const float fireballRight = fireball->m_Transform.translation.x + fireballHalf.x;
            const float fireballBottom = fireball->m_Transform.translation.y - fireballHalf.y;
            const float fireballTop = fireball->m_Transform.translation.y + fireballHalf.y;

            for (auto& enemy : m_Enemies) {
                if (!enemy->IsAlive()) continue;

                const glm::vec2 enemyHalf = enemy->GetHalfExtents();
                const float enemyLeft = enemy->m_Transform.translation.x - enemyHalf.x;
                const float enemyRight = enemy->m_Transform.translation.x + enemyHalf.x;
                const float enemyBottom = enemy->m_Transform.translation.y - enemyHalf.y;
                const float enemyTop = enemy->m_Transform.translation.y + enemyHalf.y;

                const bool overlapX = fireballRight > enemyLeft && fireballLeft < enemyRight;
                const bool overlapY = fireballTop > enemyBottom && fireballBottom < enemyTop;
                if (!overlapX || !overlapY) continue;

                enemy->Stomp();
                fireball->Explode();
                AddScore(100);
                break;
            }
        }

        if (g_MapManager && g_MapManager->HasGoal() &&
            m_GoalSequenceStage == GoalSequenceStage::None &&
            m_Mario->m_Transform.translation.x >= g_MapManager->GetGoalX()) {
            StartGoalSequence();
        }
    }

    if (m_GoalSequenceStage == GoalSequenceStage::Entering) {
        UpdateGoalSequence(dt);
    }
    if (m_GoalSequenceStage == GoalSequenceStage::PlayerControl) {
        UpdateGoalSequence(dt);
    }

    m_Enemies.erase(
        std::remove_if(m_Enemies.begin(), m_Enemies.end(),
                       [](const std::unique_ptr<Enemy>& enemy) { return enemy->IsDeadAndExpired(); }),
        m_Enemies.end()
    );
    m_Fireballs.erase(
        std::remove_if(m_Fireballs.begin(), m_Fireballs.end(),
                       [](const std::unique_ptr<Fireball>& fireball) { return fireball->IsExpired(); }),
        m_Fireballs.end()
    );
    m_Pickups.erase(
        std::remove_if(m_Pickups.begin(), m_Pickups.end(),
                       [](const std::unique_ptr<Pickup>& pickup) { return pickup->IsCollected(); }),
        m_Pickups.end()
    );
    m_Debris.erase(
        std::remove_if(m_Debris.begin(), m_Debris.end(),
                       [](const std::unique_ptr<Debris>& debris) { return debris->IsExpired(); }),
        m_Debris.end()
    );

    if (m_Mario && g_MapManager) {
        m_ViewX = m_Mario->m_Transform.translation.x;

        float mapLeft = g_MapManager->GetWorldLeft();
        float mapRight = g_MapManager->GetWorldRight();
        float halfScreen = WINDOW_WIDTH / 2.0f;
        float minViewX = mapLeft + halfScreen;
        float maxViewX = mapRight - halfScreen;

        if (minViewX > maxViewX) {
            m_ViewX = 0.0f;
        } else {
            m_ViewX = std::clamp(m_ViewX, minViewX, maxViewX);
        }
    }

    RefreshHudText();
    RenderGameplay();
}

void App::Update() {
    const float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);

    switch (m_ScreenState) {
    case ScreenState::Title:
        UpdateTitleScreen(dt);
        break;
    case ScreenState::LevelIntro:
        UpdateLevelIntro(dt);
        break;
    case ScreenState::Gameplay:
        UpdateGameplay(dt);
        break;
    }

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() {
    LOG_TRACE("End");
}
