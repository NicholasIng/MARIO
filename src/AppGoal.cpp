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

void App::ActivateNearbyEnemies() {
    if (!m_Mario) return;

    for (auto& pending : m_PendingEnemySpawns) {
        if (pending.activated) continue;
        if (std::abs(pending.position.x - m_Mario->m_Transform.translation.x) > ENEMY_SPAWN_RANGE_X) {
            continue;
        }

        m_Enemies.push_back(std::make_unique<Enemy>(pending.position.x, pending.position.y, pending.kind));
        pending.activated = true;
    }
}

void App::StartGoalSequence() {
    if (!m_Mario || !g_MapManager || m_GoalSequenceStage != GoalSequenceStage::None) return;

    m_GoalSequenceStage = GoalSequenceStage::Sliding;
    m_GoalSequenceTimer = 0.0f;
    m_GoalFlagMarioOffsetY = 0.0f;
    m_GoalCelebrationPlayed = false;
    m_Fireballs.clear();
    m_FireballCooldown = 0.0f;
    StopMusic(250);
    PlaySfx(m_Audio.flagpole.get());

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
            m_Mario->StartGoalWalk(m_CastleDoorX);
            m_GoalSequenceStage = GoalSequenceStage::PlayerControl;
        }
    } else if (m_GoalSequenceStage == GoalSequenceStage::PlayerControl) {
        m_Mario->Update();
        if (m_Mario->HasReachedGoalWalkTarget()) {
            m_Mario->m_Transform.translation.x = m_CastleDoorX;
            m_Mario->SetVisible(false);
            m_GoalSequenceStage = GoalSequenceStage::Entering;
            m_GoalSequenceTimer = 0.0f;
            if (!m_GoalCelebrationPlayed) {
                m_GoalCelebrationPlayed = true;
                PlaySfx(m_Audio.stageClear.get());
            }
        }
    } else if (m_GoalSequenceStage == GoalSequenceStage::Entering) {
        if (m_DisplayedLevelTime > 0) {
            m_GoalSequenceTimer += dt;
            while (m_GoalSequenceTimer >= TIME_BONUS_TICK_DURATION && m_DisplayedLevelTime > 0) {
                m_GoalSequenceTimer -= TIME_BONUS_TICK_DURATION;
                --m_DisplayedLevelTime;
                m_LevelTimer = static_cast<float>(m_DisplayedLevelTime);
                SetSpriteText(m_HudTimeValue, PadNumber(m_DisplayedLevelTime, 3));
                AddScore(50);
            }

            if (m_DisplayedLevelTime <= 0) {
                m_GoalSequenceTimer = GOAL_FINISH_DELAY;
            }
        } else {
            m_GoalSequenceTimer = std::max(0.0f, m_GoalSequenceTimer - dt);
            if (m_GoalSequenceTimer <= 0.0f) {
                m_GoalSequenceStage = GoalSequenceStage::Finished;
                BeginPostGoalTransition();
            }
        }
    }
}

