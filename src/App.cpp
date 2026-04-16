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

}

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
        m_CastleDoorX,
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

    Util::Color bg(
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
        bg,
        &enemySpawns
    );

    glClearColor(bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f, 1.0f);

    for (const auto& spawn : enemySpawns) {
        m_PendingEnemySpawns.push_back({ spawn, false });
    }

    // fallback if red spawn pixel is missing
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

    LOG_TRACE("Map size = {} x {}", g_MapManager->GetWidth(), g_MapManager->GetHeight());
    LOG_TRACE("Mario start pos = {}, {}",
        m_Mario->m_Transform.translation.x,
        m_Mario->m_Transform.translation.y);

    ActivateNearbyEnemies();

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    const float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);
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
                }
            }
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
            } else if (stomped) {
                enemy->Stomp();
                m_Mario->BounceAfterStomp();
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
                m_Mario->PowerUp(pickup->GetType());
                pickup->Collect();
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
                break;
            }
        }

        if (g_MapManager && g_MapManager->HasGoal() &&
            m_GoalSequenceStage == GoalSequenceStage::PlayerControl &&
            m_Mario->m_Transform.translation.x >= m_CastleDoorX) {
            m_Mario->m_Transform.translation.x = m_CastleDoorX;
            m_Mario->SetVisible(false);
            m_GoalSequenceStage = GoalSequenceStage::Entering;
            m_GoalSequenceTimer = GOAL_FINISH_DELAY;
        } else if (g_MapManager && g_MapManager->HasGoal() &&
            m_GoalSequenceStage == GoalSequenceStage::None &&
            m_Mario->m_Transform.translation.x >= g_MapManager->GetGoalX()) {
            StartGoalSequence();
        }
    }

    if (m_GoalSequenceStage == GoalSequenceStage::Entering) {
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
        // follow Mario like the youtuber's i_view_x
        m_ViewX = m_Mario->m_Transform.translation.x;

        // clamp camera so it does not scroll beyond level edges
        float mapLeft = g_MapManager->GetWorldLeft();
        float mapRight = g_MapManager->GetWorldRight();

        float halfScreen = WINDOW_WIDTH / 2.0f;

        float minViewX = mapLeft + halfScreen;
        float maxViewX = mapRight - halfScreen;

        // if level is narrower than the screen, keep camera centered
        if (minViewX > maxViewX) {
            m_ViewX = 0.0f;
        }
        else {
            m_ViewX = std::clamp(m_ViewX, minViewX, maxViewX);
        }
    }

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
        // draw Mario relative to camera, but keep real position for physics
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

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() {
    LOG_TRACE("End");
}
