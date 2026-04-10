#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "MapManager.hpp"
#include "ConvertSketch.hpp"
#include "Enemy.hpp"
#include "AssetPaths.hpp"
#include "config.hpp"
#include <algorithm>
#include <cmath>

// global pointer for Mario collision
std::unique_ptr<MapManager> g_MapManager;

void App::Start() {
    LOG_TRACE("Start");

    g_MapManager = std::make_unique<MapManager>();
    m_Mario = std::make_unique<Mario>();
    m_Enemies.clear();
    m_Pickups.clear();

    Util::Color bg(0, 255, 255, 255);
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
        m_Enemies.push_back(std::make_unique<Enemy>(spawn.x, spawn.y));
    }

    // fallback if red spawn pixel is missing
    if (!foundSpawn) {
        m_Mario->m_Transform.translation = { 0.0f, -200.0f };
    }
    m_Mario->SetSpawnPosition(m_Mario->m_Transform.translation);

    m_ViewX = 0.0f;

    LOG_TRACE("Map size = {} x {}", g_MapManager->GetWidth(), g_MapManager->GetHeight());
    LOG_TRACE("Mario start pos = {}, {}",
        m_Mario->m_Transform.translation.x,
        m_Mario->m_Transform.translation.y);

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    if (g_MapManager) {
        g_MapManager->Update();
    }
    LootType lootType;
    glm::vec2 lootPos;
    while (g_MapManager && g_MapManager->PollSpawnEvent(lootType, lootPos)) {
        m_Pickups.push_back(std::make_unique<Pickup>(lootType, lootPos.x, lootPos.y));
    }
    if (m_Mario) {
        m_Mario->Update();
    }
    for (auto& enemy : m_Enemies) {
        enemy->Update();
    }
    for (auto& pickup : m_Pickups) {
        pickup->Update();
    }

    if (m_Mario && !m_Mario->IsDead()) {
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
            const bool stomped =
                m_Mario->GetVelocityY() < 0.0f &&
                m_Mario->m_Transform.translation.y >= enemy->m_Transform.translation.y &&
                marioBottom >= enemyTop - 12.0f &&
                verticalOverlap <= horizontalOverlap;

            if (stomped) {
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
                m_Mario->PowerUp();
                pickup->Collect();
            }
        }

        if (g_MapManager && g_MapManager->HasGoal() &&
            m_Mario->m_Transform.translation.x >= g_MapManager->GetGoalX()) {
            m_CurrentState = State::END;
        }
    }

    m_Enemies.erase(
        std::remove_if(m_Enemies.begin(), m_Enemies.end(),
                       [](const std::unique_ptr<Enemy>& enemy) { return enemy->IsDeadAndExpired(); }),
        m_Enemies.end()
    );
    m_Pickups.erase(
        std::remove_if(m_Pickups.begin(), m_Pickups.end(),
                       [](const std::unique_ptr<Pickup>& pickup) { return pickup->IsCollected(); }),
        m_Pickups.end()
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

    if (m_Mario) {
        // draw Mario relative to camera, but keep real position for physics
        auto oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= m_ViewX;

        m_Mario->Draw();

        m_Mario->m_Transform.translation = oldPos;
    }
    for (auto& enemy : m_Enemies) {
        auto oldPos = enemy->m_Transform.translation;
        enemy->m_Transform.translation.x -= m_ViewX;
        enemy->Draw();
        enemy->m_Transform.translation = oldPos;
    }
    for (auto& pickup : m_Pickups) {
        auto oldPos = pickup->m_Transform.translation;
        pickup->m_Transform.translation.x -= m_ViewX;
        pickup->Draw();
        pickup->m_Transform.translation = oldPos;
    }

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() {
    LOG_TRACE("End");
}
