#include "Debris.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {

constexpr float DEBRIS_GRAVITY = -1500.0f;
constexpr float DEBRIS_SCALE = 3.0f;

std::string GetDebrisPath(Debris::Piece piece) {
    switch (piece) {
    case Debris::Piece::TopLeft:
        return AssetPaths::Image("debris_topleft.png");
    case Debris::Piece::TopRight:
        return AssetPaths::Image("debris_topright.png");
    case Debris::Piece::BottomLeft:
        return AssetPaths::Image("debris_bottomleft.png");
    case Debris::Piece::BottomRight:
    default:
        return AssetPaths::Image("debris_bottomright.png");
    }
}

glm::vec2 GetInitialVelocity(Debris::Piece piece) {
    switch (piece) {
    case Debris::Piece::TopLeft:
        return { -135.0f, 420.0f };
    case Debris::Piece::TopRight:
        return { 135.0f, 420.0f };
    case Debris::Piece::BottomLeft:
        return { -95.0f, 290.0f };
    case Debris::Piece::BottomRight:
    default:
        return { 95.0f, 290.0f };
    }
}

glm::vec2 GetSpawnOffset(Debris::Piece piece) {
    switch (piece) {
    case Debris::Piece::TopLeft:
        return { -12.0f, 12.0f };
    case Debris::Piece::TopRight:
        return { 12.0f, 12.0f };
    case Debris::Piece::BottomLeft:
        return { -12.0f, -12.0f };
    case Debris::Piece::BottomRight:
    default:
        return { 12.0f, -12.0f };
    }
}

} // namespace

Debris::Debris(float x, float y, Piece piece) {
    m_Image = std::make_shared<Util::Image>(GetDebrisPath(piece));
    SetDrawable(m_Image);

    m_Transform.translation = { x, y };
    m_Transform.translation += GetSpawnOffset(piece);
    m_Transform.scale = { DEBRIS_SCALE, DEBRIS_SCALE };
    m_ZIndex = 8.5f;

    m_Velocity = GetInitialVelocity(piece);
}

void Debris::Update() {
    if (m_Expired) return;

    const float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);

    m_Velocity.y += DEBRIS_GRAVITY * dt;
    m_Transform.translation += m_Velocity * dt;

    if (g_MapManager) {
        const float mapBottom = -(g_MapManager->GetHeight() * g_MapManager->GetTileSize()) / 2.0f;
        if (m_Transform.translation.y < mapBottom - 96.0f) {
            m_Expired = true;
            SetVisible(false);
        }
    }
}
