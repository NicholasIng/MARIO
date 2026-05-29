#pragma once

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

extern std::unique_ptr<MapManager> g_MapManager;

namespace MarioDetail {
constexpr float GOAL_SLIDE_SPEED = 220.0f;
constexpr float GOAL_CROSS_POLE_SPEED = 180.0f;
constexpr float GOAL_DROP_SPEED = 260.0f;
constexpr float GOAL_WALK_SPEED = 110.0f;
constexpr float GOAL_ENTER_DURATION = 0.4f;
constexpr float FLAG_SLIDE_OFFSET_X = -1.0f;
constexpr float POLE_EXIT_OFFSET_X = 22.0f;
constexpr float SMALL_BIG_TRANSITION_DURATION = 0.55f;
constexpr float SMALL_BIG_TRANSITION_FRAME_INTERVAL = 0.08f;
constexpr float FIRE_TRANSITION_DURATION = 0.75f;
constexpr float FIRE_TRANSITION_FRAME_INTERVAL = 0.06f;
constexpr float STAR_TRANSITION_DURATION = 0.5f;
constexpr float STAR_TRANSITION_FRAME_INTERVAL = 0.06f;
constexpr float STAR_POWER_DURATION = 10.0f;
constexpr float STAR_FLASH_FRAME_INTERVAL = 0.08f;
constexpr float SMB3_WALK_SPEED = 200.0f;
constexpr float SMB3_RUN_SPEED = 400.0f;
constexpr float SMB3_STAR_RUN_SPEED = 500.0f;
constexpr float SMB3_GROUND_ACCEL = 1500.0f;
constexpr float SMB3_GROUND_RUN_ACCEL = 1100.0f;
constexpr float SMB3_AIR_ACCEL = 900.0f;
constexpr float SMB3_STAR_ACCEL = 2700.0f;
constexpr float SMB3_RELEASE_FRICTION = 1750.0f;
constexpr float SMB3_TURN_BRAKE_DECEL = 2000.0f;
constexpr float SMB3_JUMP_HOLD_FORCE = 1700.0f;

enum class PaletteVariant {
    Normal,
    Green,
    Red,
    Black
};

struct PaletteFrameVariants {
    std::string green;
    std::string red;
    std::string black;
};

inline const std::unordered_map<std::string, PaletteFrameVariants>& PaletteVariantMap() {
    static const std::unordered_map<std::string, PaletteFrameVariants> kVariantMap = {
        { AssetPaths::Image("Character/MarioIdle.png"), {
            AssetPaths::Image("character/marioidle_green.png"),
            AssetPaths::Image("character/marioidle_red.png"),
            AssetPaths::Image("character/marioidle_black.png")
        } },
        { AssetPaths::Image("Character/MarioWalk1.png"), {
            AssetPaths::Image("character/mariowalk1_green.png"),
            AssetPaths::Image("character/mariowalk1_red.png"),
            AssetPaths::Image("character/mariowalk1_black.png")
        } },
        { AssetPaths::Image("Character/MarioWalk2.png"), {
            AssetPaths::Image("character/mariowalk2_green.png"),
            AssetPaths::Image("character/mariowalk2_red.png"),
            AssetPaths::Image("character/mariowalk2_black.png")
        } },
        { AssetPaths::Image("Character/MarioWalk3.png"), {
            AssetPaths::Image("character/mariowalk3_green.png"),
            AssetPaths::Image("character/mariowalk3_red.png"),
            AssetPaths::Image("character/mariowalk3_black.png")
        } },
        { AssetPaths::Image("Character/MarioJump.png"), {
            AssetPaths::Image("character/mariojump_green.png"),
            AssetPaths::Image("character/mariojump_red.png"),
            AssetPaths::Image("character/mariojump_black.png")
        } },
        { AssetPaths::Image("Character/MarioBrake.png"), {
            AssetPaths::Image("character/mariobrake_green.png"),
            AssetPaths::Image("character/mariobrake_red.png"),
            AssetPaths::Image("character/mariobrake_black.png")
        } },
        { AssetPaths::Image("character/Bigmario.png"), {
            AssetPaths::Image("character/bigmarioidle_green.png"),
            AssetPaths::Image("character/bigmarioidle_red.png"),
            AssetPaths::Image("character/bigmarioidle_black.png")
        } },
        { AssetPaths::Image("character/Bigmariowalk1.png"), {
            AssetPaths::Image("character/bigmariowalk1_green.png"),
            AssetPaths::Image("character/bigmariowalk1_red.png"),
            AssetPaths::Image("character/bigmariowalk1_black.png")
        } },
        { AssetPaths::Image("character/Bigmariowalk2.png"), {
            AssetPaths::Image("character/bigmariowalk2_green.png"),
            AssetPaths::Image("character/bigmariowalk2_red.png"),
            AssetPaths::Image("character/bigmariowalk2_black.png")
        } },
        { AssetPaths::Image("character/Bigmariowalk3.png"), {
            AssetPaths::Image("character/bigmariowalk3_green.png"),
            AssetPaths::Image("character/bigmariowalk3_red.png"),
            AssetPaths::Image("character/bigmariowalk3_black.png")
        } },
        { AssetPaths::Image("character/Bigmariojump.png"), {
            AssetPaths::Image("character/bigmariojump_green.png"),
            AssetPaths::Image("character/bigmariojump_red.png"),
            AssetPaths::Image("character/bigmariojump_black.png")
        } },
        { AssetPaths::Image("character/Bigmariobrake.png"), {
            AssetPaths::Image("character/bigmariobrake_green.png"),
            AssetPaths::Image("character/bigmariobrake_red.png"),
            AssetPaths::Image("character/bigmariobrake_black.png")
        } },
        { AssetPaths::Image("character/Bigmariocrouch.png"), {
            AssetPaths::Image("character/bigmariocrouch_green.png"),
            AssetPaths::Image("character/bigmariocrouch_red.png"),
            AssetPaths::Image("character/bigmariocrouch_black.png")
        } }
    };

    return kVariantMap;
}

inline bool HasPaletteVariantForPath(const std::string& basePath) {
    return PaletteVariantMap().find(basePath) != PaletteVariantMap().end();
}

inline std::string ResolvePaletteFramePath(const std::string& basePath, PaletteVariant variant) {
    if (variant == PaletteVariant::Normal) {
        return basePath;
    }

    const auto it = PaletteVariantMap().find(basePath);
    if (it == PaletteVariantMap().end()) {
        return basePath;
    }

    switch (variant) {
    case PaletteVariant::Green:
        return it->second.green;
    case PaletteVariant::Red:
        return it->second.red;
    case PaletteVariant::Black:
        return it->second.black;
    case PaletteVariant::Normal:
    default:
        return basePath;
    }
}

inline PaletteVariant FireTransitionPaletteVariant(float elapsed) {
    static const std::array<PaletteVariant, 6> kSequence = {
        PaletteVariant::Normal,
        PaletteVariant::Green,
        PaletteVariant::Red,
        PaletteVariant::Black,
        PaletteVariant::Red,
        PaletteVariant::Green
    };

    const int index = static_cast<int>(elapsed / FIRE_TRANSITION_FRAME_INTERVAL) % static_cast<int>(kSequence.size());
    return kSequence[index];
}

inline PaletteVariant StarPaletteVariant(float elapsed) {
    static const std::array<PaletteVariant, 4> kSequence = {
        PaletteVariant::Normal,
        PaletteVariant::Green,
        PaletteVariant::Red,
        PaletteVariant::Black
    };

    const int index = static_cast<int>(elapsed / STAR_FLASH_FRAME_INTERVAL) % static_cast<int>(kSequence.size());
    return kSequence[index];
}

inline PaletteVariant StarTransitionPaletteVariant(float elapsed) {
    static const std::array<PaletteVariant, 6> kSequence = {
        PaletteVariant::Normal,
        PaletteVariant::Green,
        PaletteVariant::Red,
        PaletteVariant::Black,
        PaletteVariant::Red,
        PaletteVariant::Green
    };

    const int index = static_cast<int>(elapsed / STAR_TRANSITION_FRAME_INTERVAL) % static_cast<int>(kSequence.size());
    return kSequence[index];
}

inline bool IntersectsSolidTile(const glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return false;

    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = -(g_MapManager->GetWidth() * tileSize) / 2.0f;
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

inline void ResolveMinorGroundPenetration(glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;
    if (!IntersectsSolidTile(center, halfExtents)) return;

    glm::vec2 candidate = center;
    constexpr int kMaxMinorSnapSteps = 8;
    for (int step = 0; step < kMaxMinorSnapSteps; ++step) {
        candidate.y += 1.0f;
        if (!IntersectsSolidTile(candidate, halfExtents)) {
            center = candidate;
            return;
        }
    }
}

inline void ClampGrowthToAvailableHeadroom(glm::vec2& center, float originalY, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;

    while (center.y > originalY && IntersectsSolidTile(center, halfExtents)) {
        center.y -= 1.0f;
    }
}

} // namespace MarioDetail
