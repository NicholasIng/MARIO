#include "Fireball.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {

constexpr float FIREBALL_SPEED = 330.0f;
constexpr float FIREBALL_GRAVITY = -1600.0f;
constexpr float FIREBALL_BOUNCE_SPEED = 520.0f;
constexpr float FIREBALL_LIFETIME = 0.24f;

}

Fireball::Fireball(float x, float y, float direction)
    : m_Direction(direction >= 0.0f ? 1.0f : -1.0f) {
    std::vector<std::string> flyingFrames = {
        AssetPaths::Image("fireball_bounce1.png"),
        AssetPaths::Image("fireball_bounce2.png"),
        AssetPaths::Image("fireball_bounce3.png"),
        AssetPaths::Image("fireball_bounce4.png")
    };
    std::vector<std::string> hitFrames = {
        AssetPaths::Image("fireball_hit1.png"),
        AssetPaths::Image("fireball_hit2.png"),
        AssetPaths::Image("fireball_hit3.png")
    };

    m_FlyingAnimation = std::make_unique<Animation>(flyingFrames, 0.05f);
    m_HitAnimation = std::make_unique<Animation>(hitFrames, 0.05f);
    m_Image = std::make_shared<Util::Image>(flyingFrames.front());
    SetDrawable(m_Image);

    m_Transform.translation = { x, y };
    m_Transform.scale = { 2.2f * m_Direction, 2.2f };
    m_ZIndex = 9.5f;
    m_VelocityY = 120.0f;
}

void Fireball::Update() {
    if (m_Expired) return;

    const float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);
    if (m_Exploding) {
        m_HitTimer -= dt;
        m_HitAnimation->Update(dt);
        m_Image->SetImage(m_HitAnimation->GetCurrentFramePath());
        if (m_HitTimer <= 0.0f) {
            m_Expired = true;
            SetVisible(false);
        }
        return;
    }

    m_FlyingAnimation->Update(dt);
    m_Image->SetImage(m_FlyingAnimation->GetCurrentFramePath());

    if (!g_MapManager) {
        m_Transform.translation.x += m_Direction * FIREBALL_SPEED * dt;
        return;
    }

    const glm::vec2 half = GetHalfExtents();
    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapRight = g_MapManager->GetWorldRight();
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

    m_VelocityY += FIREBALL_GRAVITY * dt;

    float candidateX = m_Transform.translation.x + m_Direction * FIREBALL_SPEED * dt;
    float candidateY = m_Transform.translation.y + m_VelocityY * dt;

    const float topY = m_Transform.translation.y + half.y - eps;
    const float bottomY = m_Transform.translation.y - half.y + eps;
    const int topGridY = std::clamp(worldToGridY(topY - eps), 0, std::max(0, mapHeight - 1));
    const int bottomGridY = std::clamp(worldToGridY(bottomY + eps), 0, std::max(0, mapHeight - 1));

    if (m_Direction > 0.0f) {
        const float rightEdge = candidateX + half.x;
        const int gridX = std::clamp(worldToGridX(rightEdge - eps), 0, std::max(0, mapWidth - 1));
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gridX, gy)) {
                Explode();
                return;
            }
        }
    } else {
        const float leftEdge = candidateX - half.x;
        const int gridX = std::clamp(worldToGridX(leftEdge + eps), 0, std::max(0, mapWidth - 1));
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gridX, gy)) {
                Explode();
                return;
            }
        }
    }

    const float leftX = candidateX - half.x;
    const float rightX = candidateX + half.x;
    const int leftGridX = std::clamp(worldToGridX(leftX + eps), 0, std::max(0, mapWidth - 1));
    const int rightGridX = std::clamp(worldToGridX(rightX - eps), 0, std::max(0, mapWidth - 1));

    if (m_VelocityY <= 0.0f) {
        const float bottomEdge = candidateY - half.y;
        const int gridY = std::clamp(worldToGridY(bottomEdge + eps), 0, std::max(0, mapHeight - 1));
        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            if (g_MapManager->IsSolidAt(gx, gridY)) {
                const float tileTop = mapTop - gridY * tileSize;
                candidateY = tileTop + half.y;
                m_VelocityY = FIREBALL_BOUNCE_SPEED;
                break;
            }
        }
    } else {
        const float topEdge = candidateY + half.y;
        const int gridY = std::clamp(worldToGridY(topEdge + eps), 0, std::max(0, mapHeight - 1));
        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            if (g_MapManager->IsSolidAt(gx, gridY)) {
                Explode();
                return;
            }
        }
    }

    m_Transform.translation = { candidateX, candidateY };

    if (m_Transform.translation.x < mapLeft - 64.0f || m_Transform.translation.x > mapRight + 64.0f) {
        m_Expired = true;
        SetVisible(false);
    }
}

void Fireball::Explode() {
    if (m_Exploding || m_Expired) return;
    m_Exploding = true;
    m_HitTimer = FIREBALL_LIFETIME;
    m_HitAnimation->Reset();
    m_Image->SetImage(m_HitAnimation->GetCurrentFramePath());
    m_Transform.scale = { 2.2f, 2.2f };
}
