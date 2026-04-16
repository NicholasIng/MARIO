#include "Enemy.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {
constexpr float FLIPPED_DEATH_GRAVITY = -1800.0f;
constexpr float FLIPPED_DEATH_LAUNCH_Y = 520.0f;
constexpr float FLIPPED_DEATH_CULL_MARGIN = 96.0f;

bool IntersectsSolidTile(const glm::vec2& center, const glm::vec2& halfExtents) {
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

bool IsSolidAtWorld(float worldX, float worldY) {
    if (!g_MapManager) return false;

    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;

    const int gridX = static_cast<int>(std::floor((worldX - mapLeft) / tileSize));
    const int gridY = static_cast<int>(std::floor((mapTop - worldY) / tileSize));
    return g_MapManager->IsSolidAt(gridX, gridY);
}

void SnapUpOutOfGround(glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;

    const int maxSteps = static_cast<int>(g_MapManager->GetTileSize() * 3.0f);
    for (int step = 0; step < maxSteps && IntersectsSolidTile(center, halfExtents); ++step) {
        center.y += 1.0f;
    }
}

} // namespace

Enemy::Enemy(float x, float y)
    : m_LeftPath(AssetPaths::Image("Goomba_l.png")),
      m_RightPath(AssetPaths::Image("Goomba_r.png")),
      m_DeathPath(AssetPaths::Image("Goombadeath.png")) {
    m_Image = std::make_shared<Util::Image>(m_LeftPath);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 9.0f;
}

void Enemy::SetDirection(float direction) {
    if (direction == 0.0f) return;
    m_Direction = (direction > 0.0f) ? 1.0f : -1.0f;
    RefreshSprite();
}

void Enemy::Update() {
    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;

    if (!m_Alive) {
        if (m_FlippedDeath) {
            m_VelocityY += FLIPPED_DEATH_GRAVITY * dt;
            m_Transform.translation.x += m_VelocityX * dt;
            m_Transform.translation.y += m_VelocityY * dt;

            const float mapBottom = g_MapManager
                ? -(g_MapManager->GetHeight() * g_MapManager->GetTileSize()) / 2.0f
                : -1000.0f;
            if (m_Transform.translation.y + GetHalfExtents().y < mapBottom - FLIPPED_DEATH_CULL_MARGIN) {
                m_DeathFinished = true;
            }
            return;
        }

        m_DeathTimer -= dt;
        if (m_DeathTimer <= 0.0f) {
            m_DeathFinished = true;
        }
        return;
    }

    SnapUpOutOfGround(m_Transform.translation, GetHalfExtents());
    if (!g_MapManager) {
        m_Transform.translation.x += m_Direction * m_Speed * dt;
        RefreshSprite();
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
    bool grounded = false;
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
                grounded = true;
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

    if (grounded) {
        const float frontX = candidateX + m_Direction * (half.x + 2.0f);
        const float footY = m_Transform.translation.y - half.y - 2.0f;
        if (!IsSolidAtWorld(frontX, footY)) {
            m_Direction *= -1.0f;
            RefreshSprite();
            return;
        }
    }

    m_Transform.translation.x = candidateX;
    RefreshSprite();
}

void Enemy::Stomp() {
    m_Alive = false;
    m_FlippedDeath = false;
    m_DeathFinished = false;
    m_VelocityX = 0.0f;
    m_DeathTimer = 0.5f;
    m_Image->SetImage(m_DeathPath);
}

void Enemy::KillFlipped(float horizontalVelocity) {
    m_Alive = false;
    m_FlippedDeath = true;
    m_DeathFinished = false;
    m_DeathTimer = 0.0f;
    m_VelocityX = horizontalVelocity;
    m_VelocityY = FLIPPED_DEATH_LAUNCH_Y;
    m_Transform.scale.x = -std::abs(m_Transform.scale.x);
    m_Transform.scale.y = -std::abs(m_Transform.scale.y);
    m_Image->SetImage(m_UseLeftWalkFrame ? m_LeftPath : m_RightPath);
}

void Enemy::RefreshSprite() {
    constexpr float walkFrameDuration = 0.12f;
    m_WalkAnimationTimer += Util::Time::GetDeltaTimeMs() / 1000.0f;
    if (m_WalkAnimationTimer >= walkFrameDuration) {
        m_WalkAnimationTimer = std::fmod(m_WalkAnimationTimer, walkFrameDuration);
        m_UseLeftWalkFrame = !m_UseLeftWalkFrame;
    }

    const std::string& path = m_UseLeftWalkFrame ? m_LeftPath : m_RightPath;
    m_Image->SetImage(path);
}
