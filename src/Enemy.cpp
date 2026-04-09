#include "Enemy.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

extern std::unique_ptr<MapManager> g_MapManager;

Enemy::Enemy(float x, float y)
    : m_StartX(x),
      m_LeftPath(AssetPaths::Image("Goomba_l.png")),
      m_RightPath(AssetPaths::Image("Goomba_r.png")),
      m_DeathPath(AssetPaths::Image("Goombadeath.png")) {
    m_Image = std::make_shared<Util::Image>(m_LeftPath);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 9.0f;
}

void Enemy::Update() {
    if (!m_Alive) {
        m_DeathTimer -= Util::Time::GetDeltaTimeMs() / 1000.0f;
        return;
    }

    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;
    if (!g_MapManager) {
        m_Transform.translation.x += m_Direction * m_Speed * dt;
        return;
    }

    const glm::vec2 half = GetHalfExtents();
    const float gravity = -1800.0f;
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
        int gridY = std::clamp(worldToGridY(bottomEdge - eps), 0, std::max(0, mapHeight - 1));
        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            if (g_MapManager->IsSolidAt(gx, gridY)) {
                float tileTop = mapTop - gridY * tileSize;
                candidateY = tileTop + half.y;
                m_VelocityY = 0.0f;
                break;
            }
        }
    }
    m_Transform.translation.y = candidateY;

    float candidateX = m_Transform.translation.x + m_Direction * m_Speed * dt;
    float topY = m_Transform.translation.y + half.y - eps;
    float bottomY = m_Transform.translation.y - half.y + eps;
    int topGridY = std::clamp(worldToGridY(topY - eps), 0, std::max(0, mapHeight - 1));
    int bottomGridY = std::clamp(worldToGridY(bottomY + eps), 0, std::max(0, mapHeight - 1));

    if (m_Direction > 0.0f) {
        float rightEdge = candidateX + half.x;
        int gridX = std::clamp(worldToGridX(rightEdge - eps), 0, std::max(0, mapWidth - 1));
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gridX, gy)) {
                m_Direction = -1.0f;
                RefreshSprite();
                return;
            }
        }
    } else {
        float leftEdge = candidateX - half.x;
        int gridX = std::clamp(worldToGridX(leftEdge + eps), 0, std::max(0, mapWidth - 1));
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gridX, gy)) {
                m_Direction = 1.0f;
                RefreshSprite();
                return;
            }
        }
    }

    m_Transform.translation.x = candidateX;
}

void Enemy::Stomp() {
    m_Alive = false;
    m_DeathTimer = 0.5f;
    m_Image->SetImage(m_DeathPath);
}

void Enemy::RefreshSprite() {
    const std::string& path = (m_Direction < 0.0f) ? m_LeftPath : m_RightPath;
    m_Image->SetImage(path);
}
