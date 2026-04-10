#include "Pickup.hpp"
#include "AssetPaths.hpp"

#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

extern std::unique_ptr<MapManager> g_MapManager;

Pickup::Pickup(LootType type, float x, float y)
    : m_Type(type) {
    const std::string path = (type == LootType::RedMushroom)
        ? AssetPaths::Image("Mushroom_red.png")
        : AssetPaths::Image("Mushroom_green.png");

    m_Image = std::make_shared<Util::Image>(path);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 8.0f;
}

void Pickup::Update() {
    if (m_Collected) return;

    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;
    if (m_RiseRemaining > 0.0f) {
        const float rise = std::min(m_RiseRemaining, 48.0f * dt);
        m_Transform.translation.y += rise;
        m_RiseRemaining -= rise;
        return;
    }

    if (!g_MapManager) return;

    const float gravity = -1800.0f;
    const float moveSpeed = 90.0f;
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

    m_VelocityY += gravity * dt;

    float candidateY = m_Transform.translation.y + m_VelocityY * dt;
    float leftX = m_Transform.translation.x - half.x;
    float rightX = m_Transform.translation.x + half.x;
    int leftGridX = std::clamp(worldToGridX(leftX + eps), 0, std::max(0, mapWidth - 1));
    int rightGridX = std::clamp(worldToGridX(rightX - eps), 0, std::max(0, mapWidth - 1));

    if (m_VelocityY <= 0.0f) {
        float bottomEdge = candidateY - half.y;
        int gridY = std::clamp(worldToGridY(bottomEdge + eps), 0, std::max(0, mapHeight - 1));
        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            if (g_MapManager->IsSolidAt(gx, gridY)) {
                float tileTop = mapTop - gridY * tileSize;
                candidateY = tileTop + half.y;
                m_VelocityY = 0.0f;
                break;
            }
        }
    } else {
        float topEdge = candidateY + half.y;
        int gridY = std::clamp(worldToGridY(topEdge + eps), 0, std::max(0, mapHeight - 1));
        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            if (g_MapManager->IsSolidAt(gx, gridY)) {
                float tileBottom = mapTop - (gridY + 1) * tileSize;
                candidateY = tileBottom - half.y - eps;
                m_VelocityY = 0.0f;
                break;
            }
        }
    }
    m_Transform.translation.y = candidateY;

    float candidateX = m_Transform.translation.x + m_HorizontalDirection * moveSpeed * dt;
    float topY = m_Transform.translation.y + half.y - eps;
    float bottomY = m_Transform.translation.y - half.y + eps;
    int topGridY = std::clamp(worldToGridY(topY - eps), 0, std::max(0, mapHeight - 1));
    int bottomGridY = std::clamp(worldToGridY(bottomY + eps), 0, std::max(0, mapHeight - 1));

    if (m_HorizontalDirection > 0.0f) {
        float rightEdge = candidateX + half.x;
        int gridX = std::clamp(worldToGridX(rightEdge - eps), 0, std::max(0, mapWidth - 1));
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gridX, gy)) {
                m_HorizontalDirection = -1.0f;
                return;
            }
        }
    } else {
        float leftEdge = candidateX - half.x;
        int gridX = std::clamp(worldToGridX(leftEdge + eps), 0, std::max(0, mapWidth - 1));
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gridX, gy)) {
                m_HorizontalDirection = 1.0f;
                return;
            }
        }
    }

    m_Transform.translation.x = candidateX;
}
