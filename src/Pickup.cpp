#include "Pickup.hpp"
#include "AssetPaths.hpp"

#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

extern std::unique_ptr<MapManager> g_MapManager;

Pickup::Pickup(LootType type, float x, float y, bool useQuestionCoinSprites, bool enableLaunchHop)
    : m_Type(type), m_UseQuestionCoinSprites(useQuestionCoinSprites), m_EnableLaunchHop(enableLaunchHop) {
    std::vector<std::string> frames;
    if (type == LootType::RedMushroom) {
        frames = { AssetPaths::Image("Mushroom_red.png") };
    } else if (type == LootType::GreenMushroom) {
        frames = { AssetPaths::Image("Mushroom_green.png") };
    } else if (type == LootType::Star) {
        frames = {
            AssetPaths::Image("stars1.png"),
            AssetPaths::Image("stars2.png"),
            AssetPaths::Image("stars3.png"),
            AssetPaths::Image("stars4.png")
        };
    } else if (type == LootType::FireFlower) {
        frames = {
            AssetPaths::Image("flower1.png"),
            AssetPaths::Image("flower2.png"),
            AssetPaths::Image("flower3.png"),
            AssetPaths::Image("flower4.png")
        };
    } else {
        frames = {
            AssetPaths::Image(m_UseQuestionCoinSprites ? "q_coin1.png" : "coin1.png"),
            AssetPaths::Image(m_UseQuestionCoinSprites ? "q_coin2.png" : "coin2.png"),
            AssetPaths::Image(m_UseQuestionCoinSprites ? "q_coin3.png" : "coin3.png"),
            AssetPaths::Image(m_UseQuestionCoinSprites ? "q_coin4.png" : "coin4.png")
        };
    }

    m_Image = std::make_shared<GameImage>(frames.front());
    m_Animation = std::make_unique<Animation>(
        frames,
        (type == LootType::FireFlower || type == LootType::Coin || type == LootType::Star) ? 0.1f : 1.0f
    );
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_SpawnStartX = x;
    m_SpawnStartY = y;
    m_Transform.scale = { BASE_SCALE, 0.01f };
    m_ZIndex = 8.0f;
}

bool Pickup::ConsumeAutoAward() {
    if (!m_AutoAwardPending) return false;
    m_AutoAwardPending = false;
    return true;
}

void Pickup::Update() {
    if (m_Collected) return;

    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;
    if (m_Animation) {
        m_Animation->Update(dt);
        m_Image->SetImage(m_Animation->GetCurrentFramePath());
    }

    if (m_Type == LootType::Coin) {
        UpdateCoinPop(dt);
        return;
    }

    if (m_RiseElapsed < RISE_DURATION) {
        UpdateRise(dt);
        if (m_RiseElapsed < RISE_DURATION) {
            return;
        }
    }

    if (!m_LaunchHopStarted) {
        StartLaunchHop();
    }

    if (!g_MapManager) return;

    const float gravity = -1800.0f;
    const float moveSpeed = (m_Type == LootType::Star) ? 155.0f : 80.0f;
    const glm::vec2 half = GetHalfExtents();
    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
    const int mapWidth = g_MapManager->GetWidth();
    const int mapHeight = g_MapManager->GetHeight();
    const float eps = 0.001f;

    auto worldToGridX = [&](float worldX) {
        return static_cast<int>(std::floor((worldX - mapLeft) / tileSize));
    };
    auto worldToGridY = [&](float worldY) {
        return static_cast<int>(std::floor((mapTop - worldY) / tileSize));
    };
    const auto isInsideMap = [&](int gridX, int gridY) {
        return gridX >= 0 && gridX < mapWidth && gridY >= 0 && gridY < mapHeight;
    };
    const auto isSolidAtWorld = [&](float worldX, float worldY) {
        const int gridX = worldToGridX(worldX);
        const int gridY = worldToGridY(worldY);
        return isInsideMap(gridX, gridY) && g_MapManager->IsSolidAt(gridX, gridY);
    };
    const auto hasGroundSupport = [&](float centerX, float centerY) {
        const float probeY = centerY - half.y - 1.0f;
        return isSolidAtWorld(centerX - half.x * 0.35f, probeY) ||
               isSolidAtWorld(centerX + half.x * 0.35f, probeY);
    };

    if (!m_HasLanded || !hasGroundSupport(m_Transform.translation.x, m_Transform.translation.y)) {
        m_HasLanded = false;
        m_VelocityY += gravity * dt;
    } else {
        m_VelocityY = std::min(0.0f, m_VelocityY);
    }

    float candidateY = m_Transform.translation.y + m_VelocityY * dt;
    int leftGridX = std::max(0, worldToGridX(m_Transform.translation.x - half.x + 2.0f));
    int rightGridX = std::min(mapWidth - 1, worldToGridX(m_Transform.translation.x + half.x - 2.0f));
    if (leftGridX > rightGridX) {
        leftGridX = rightGridX = std::clamp(worldToGridX(m_Transform.translation.x), 0, std::max(0, mapWidth - 1));
    }

    if (m_VelocityY <= 0.0f) {
        // Use the mushroom's feet for floor collisions. Using the full sprite
        // width here lets its outer edge cling to the previous block and snap
        // back up instead of falling through a one-tile gap.
        const float footHalfWidth = half.x * 0.35f;
        int landingLeftGridX = std::max(
            0,
            worldToGridX(m_Transform.translation.x - footHalfWidth)
        );
        int landingRightGridX = std::min(
            mapWidth - 1,
            worldToGridX(m_Transform.translation.x + footHalfWidth)
        );
        if (landingLeftGridX > landingRightGridX) {
            landingLeftGridX = landingRightGridX = std::clamp(
                worldToGridX(m_Transform.translation.x),
                0,
                std::max(0, mapWidth - 1)
            );
        }

        const int gridY = worldToGridY(candidateY - half.y + eps);
        if (gridY >= 0 && gridY < mapHeight) {
            for (int gx = landingLeftGridX; gx <= landingRightGridX; ++gx) {
                if (m_LaunchHopActive && gridY == m_IgnoredSpawnSupportGridY) {
                    continue;
                }
                if (g_MapManager->IsSolidAt(gx, gridY)) {
                    const float tileTop = mapTop - gridY * tileSize;
                    candidateY = tileTop + half.y;
                    m_VelocityY = 0.0f;
                    m_HasLanded = true;
                    m_LaunchHopActive = false;
                    break;
                }
            }
        }
    } else {
        const int gridY = worldToGridY(candidateY + half.y + eps);
        if (gridY >= 0 && gridY < mapHeight) {
            for (int gx = leftGridX; gx <= rightGridX; ++gx) {
                if (g_MapManager->IsSolidAt(gx, gridY)) {
                    const float tileBottom = mapTop - (gridY + 1) * tileSize;
                    candidateY = tileBottom - half.y - eps;
                    m_VelocityY = 0.0f;
                    break;
                }
            }
        }
    }
    m_Transform.translation.y = candidateY;

    if (m_Type == LootType::FireFlower) {
        if (m_HasLanded && hasGroundSupport(m_Transform.translation.x, m_Transform.translation.y)) {
            m_VelocityY = 0.0f;
        } else {
            m_HasLanded = false;
        }
        return;
    }

    if (!hasGroundSupport(m_Transform.translation.x, m_Transform.translation.y)) {
        m_HasLanded = false;
    }

    const float candidateX = m_Transform.translation.x + m_HorizontalDirection * moveSpeed * dt;

    // When the pickup is standing on top of blocks, the support row should
    // not count as a side wall. That lets mushrooms and stars slide across
    // block tops naturally and only fall once the support disappears.
    int topGridY = worldToGridY(m_Transform.translation.y + half.y - 4.0f);
    int supportGridY = worldToGridY(m_Transform.translation.y - half.y - 1.0f);
    if (topGridY > supportGridY) {
        std::swap(topGridY, supportGridY);
    }
    topGridY = std::clamp(topGridY, 0, mapHeight - 1);
    supportGridY = std::clamp(supportGridY, 0, mapHeight - 1);

    if (m_HorizontalDirection > 0.0f) {
        const int gridX = worldToGridX(candidateX + half.x - eps);
        if (gridX >= mapWidth) {
            m_HorizontalDirection = -1.0f;
            return;
        }
        if (gridX >= 0) {
            for (int gy = topGridY; gy < supportGridY; ++gy) {
                if (g_MapManager->IsSolidAt(gridX, gy)) {
                    m_HorizontalDirection = -1.0f;
                    return;
                }
            }
        }
    } else {
        const int gridX = worldToGridX(candidateX - half.x + eps);
        if (gridX < 0) {
            m_HorizontalDirection = 1.0f;
            return;
        }
        if (gridX < mapWidth) {
            for (int gy = topGridY; gy < supportGridY; ++gy) {
                if (g_MapManager->IsSolidAt(gridX, gy)) {
                    m_HorizontalDirection = 1.0f;
                    return;
                }
            }
        }
    }

    m_Transform.translation.x = candidateX;
}

void Pickup::UpdateRise(float dt) {
    m_RiseElapsed = std::min(RISE_DURATION, m_RiseElapsed + dt);
    const float progress = (RISE_DURATION <= 0.0f) ? 1.0f : (m_RiseElapsed / RISE_DURATION);

    // Let pickups emerge straight out of the block first.
    const float visibleScaleY = std::max(0.01f, BASE_SCALE * progress);
    m_Transform.scale = { BASE_SCALE, visibleScaleY };
    m_Transform.translation.x = m_SpawnStartX;
    m_Transform.translation.y = m_SpawnStartY + RISE_DISTANCE * progress * 0.5f;
}

void Pickup::StartLaunchHop() {
    m_LaunchHopStarted = true;
    m_Transform.scale = { BASE_SCALE, BASE_SCALE };

    if (m_Type == LootType::FireFlower) {
        return;
    }

    if (!m_EnableLaunchHop ||
        (m_Type != LootType::RedMushroom && m_Type != LootType::GreenMushroom)) {
        return;
    }

    m_LaunchHopActive = true;
    m_HasLanded = false;
    m_VelocityY = 420.0f;

    if (g_MapManager) {
        const float tileSize = g_MapManager->GetTileSize();
        const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
        m_IgnoredSpawnSupportGridY = static_cast<int>(std::floor((mapTop - (m_SpawnStartY - 1.0f)) / tileSize));
    }
}

void Pickup::UpdateCoinPop(float dt) {
    m_CoinPopElapsed = std::min(COIN_POP_DURATION, m_CoinPopElapsed + dt);
    const float progress = std::clamp(m_CoinPopElapsed / COIN_POP_DURATION, 0.0f, 1.0f);

    // SMB3 reward coins launch directly from the block mouth, reach a high
    // peak, then drop back into the same spot before disappearing.
    const float blockMouthY = m_SpawnStartY + 6.0f;
    const float arc = 4.0f * progress * (1.0f - progress);
    m_Transform.scale = { BASE_SCALE, BASE_SCALE };
    m_Transform.translation.y = blockMouthY + COIN_POP_HEIGHT * arc;

    if (progress >= 1.0f) {
        m_Transform.translation.y = blockMouthY;
        m_AutoAwardPending = true;
        Collect();
    }
}
