#pragma once

#include "MapManager.hpp"
#include <algorithm>
#include <cmath>
#include <memory>

extern std::unique_ptr<MapManager> g_MapManager;

namespace EnemyDetail {
constexpr float FLIPPED_DEATH_GRAVITY = -1800.0f;
constexpr float FLIPPED_DEATH_LAUNCH_Y = 520.0f;
constexpr float FLIPPED_DEATH_CULL_MARGIN = 96.0f;
constexpr float ENEMY_GRAVITY = -1800.0f;
constexpr float GOOMBA_WALK_SPEED = 40.0f;
constexpr float KOOPA_WALK_SPEED = 46.0f;
constexpr float KOOPA_SHELL_SPEED = 240.0f;
constexpr float KOOPA_SHELL_IDLE_DURATION = 4.0f;
constexpr float KOOPA_RECOVERY_DURATION = 1.1f;

inline bool IntersectsSolidTile(const glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return false;

    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
    const float eps = 0.001f;

    const int leftGridX = std::clamp(
        static_cast<int>(std::floor((center.x - halfExtents.x - mapLeft + eps) / tileSize)),
        0, std::max(0, g_MapManager->GetWidth() - 1)
    );
    const int rightGridX = std::clamp(
        static_cast<int>(std::floor((center.x + halfExtents.x - mapLeft - eps) / tileSize)),
        0, std::max(0, g_MapManager->GetWidth() - 1)
    );
    const int topGridY = std::clamp(
        static_cast<int>(std::floor((mapTop - (center.y + halfExtents.y - eps)) / tileSize)),
        0, std::max(0, g_MapManager->GetHeight() - 1)
    );
    const int bottomGridY = std::clamp(
        static_cast<int>(std::floor((mapTop - (center.y - halfExtents.y + eps)) / tileSize)),
        0, std::max(0, g_MapManager->GetHeight() - 1)
    );

    for (int gx = leftGridX; gx <= rightGridX; ++gx) {
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gx, gy)) {
                return true;
            }
        }
    }

    return false;
}

inline void SnapUpOutOfGround(glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;

    const int maxSteps = static_cast<int>(g_MapManager->GetTileSize() * 3.0f);
    for (int step = 0; step < maxSteps && IntersectsSolidTile(center, halfExtents); ++step) {
        center.y += 1.0f;
    }
}

} // namespace EnemyDetail
