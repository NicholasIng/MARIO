#include "Mario.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include "AssetPaths.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {
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

const std::unordered_map<std::string, PaletteFrameVariants>& PaletteVariantMap() {
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

bool HasPaletteVariantForPath(const std::string& basePath) {
    return PaletteVariantMap().find(basePath) != PaletteVariantMap().end();
}

std::string ResolvePaletteFramePath(const std::string& basePath, PaletteVariant variant) {
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

PaletteVariant FireTransitionPaletteVariant(float elapsed) {
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

PaletteVariant StarPaletteVariant(float elapsed) {
    static const std::array<PaletteVariant, 4> kSequence = {
        PaletteVariant::Normal,
        PaletteVariant::Green,
        PaletteVariant::Red,
        PaletteVariant::Black
    };

    const int index = static_cast<int>(elapsed / STAR_FLASH_FRAME_INTERVAL) % static_cast<int>(kSequence.size());
    return kSequence[index];
}

PaletteVariant StarTransitionPaletteVariant(float elapsed) {
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

bool IntersectsSolidTile(const glm::vec2& center, const glm::vec2& halfExtents) {
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

void SnapUpOutOfGround(glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;

    const int maxSteps = static_cast<int>(g_MapManager->GetTileSize() * 3.0f);
    for (int step = 0; step < maxSteps && IntersectsSolidTile(center, halfExtents); ++step) {
        center.y += 1.0f;
    }
}

void ClampGrowthToAvailableHeadroom(glm::vec2& center, float originalY, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;

    while (center.y > originalY && IntersectsSolidTile(center, halfExtents)) {
        center.y -= 1.0f;
    }
}

} // namespace

Mario::Mario()
    : m_VelocityX(0.0f),
      m_VelocityY(0.0f),
      m_Acceleration(2000.0f),
      m_MaxSpeed(350.0f),
      m_Friction(1000.0f),
      m_Gravity(-2200.0f),
      m_JumpForce(700.0f),
      m_OnGround(false),
      m_JumpTimer(0.0f),
      m_MaxJumpTime(0.25f) {

    m_SmallAnimations[AnimState::IDLE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("Character/MarioIdle.png")
    }, 1.0f
    );

    m_SmallAnimations[AnimState::WALK] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("Character/MarioWalk1.png"),
            AssetPaths::Image("Character/MarioWalk2.png"),
            AssetPaths::Image("Character/MarioWalk3.png")
    }, 0.07f
    );

    m_SmallAnimations[AnimState::JUMP] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("Character/MarioJump.png")
    }, 1.0f
    );

    m_SmallAnimations[AnimState::BRAKE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("Character/MarioBrake.png")
    }, 1.0f
    );

    m_SmallAnimations[AnimState::CROUCH] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("Character/MarioCrouch.png")
    }, 1.0f
    );

    m_BigAnimations[AnimState::IDLE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/Bigmario.png")
    }, 1.0f
    );

    m_BigAnimations[AnimState::WALK] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/Bigmariowalk1.png"),
            AssetPaths::Image("character/Bigmariowalk2.png"),
            AssetPaths::Image("character/Bigmariowalk3.png")
    }, 0.07f
    );

    m_BigAnimations[AnimState::JUMP] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/Bigmariojump.png")
    }, 1.0f
    );

    m_BigAnimations[AnimState::BRAKE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/Bigmariobrake.png")
    }, 1.0f
    );

    m_BigAnimations[AnimState::CROUCH] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/Bigmariocrouch.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::IDLE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/bigmarioidle_fire.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::WALK] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/bigmariowalk1_fire.png"),
            AssetPaths::Image("character/bigmariowalk2_fire.png"),
            AssetPaths::Image("character/bigmariowalk3_fire.png")
    }, 0.07f
    );

    m_FireAnimations[AnimState::JUMP] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/bigmariojump_fire.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::BRAKE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/bigmariobrake_fire.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::CROUCH] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/bigmariocrouch_fire.png")
    }, 1.0f
    );

    m_FlagAnimations[PowerState::Small] = std::make_unique<Animation>(
        std::vector<std::string>{
            AssetPaths::Image("character/Marioflag1.png"),
            AssetPaths::Image("character/Marioflag2.png")
        }, 0.12f
    );
    m_FlagAnimations[PowerState::Big] = std::make_unique<Animation>(
        std::vector<std::string>{
            AssetPaths::Image("character/Bigmarioflag1.png"),
            AssetPaths::Image("character/Bigmarioflag2.png")
        }, 0.12f
    );
    m_FlagAnimations[PowerState::Fire] = std::make_unique<Animation>(
        std::vector<std::string>{
            AssetPaths::Image("character/bigmarioflag1_fire.png"),
            AssetPaths::Image("character/bigmarioflag2_fire.png")
        }, 0.12f
    );

    m_SizeTransitionFramePath = AssetPaths::Image("character/mario_transition.png");
    m_DeathFramePath = AssetPaths::Image("character/mario_die.png");
    m_TransformAnimation = std::make_unique<Animation>(
        std::vector<std::string>{
            AssetPaths::Image("character/mario_transformation1.png"),
            AssetPaths::Image("character/mario_transformation2.png"),
            AssetPaths::Image("character/mario_transformation3.png")
        }, 0.1f
    );

    m_Image = std::make_shared<Util::Image>(
        ActiveAnimations().at(AnimState::IDLE)->GetCurrentFramePath()
    );
    SetDrawable(m_Image);

    m_Transform.translation = m_SpawnPosition;
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 10.0f;
}

glm::vec2 Mario::GetHalfExtents() const {
    if (m_IsCrouching && IsBig()) {
        return { BIG_HALF_WIDTH, CROUCH_HALF_HEIGHT };
    }
    if (IsBig() || m_TransformType == TransformType::SmallBigTransition) {
        return { BIG_HALF_WIDTH, BIG_HALF_HEIGHT };
    }
    return { SMALL_HALF_WIDTH, SMALL_HALF_HEIGHT };
}

float Mario::GetRenderOffsetY() const {
    if (m_TransformType == TransformType::SmallBigTransition) {
        return m_TransformShowBigFrame ? BIG_RENDER_OFFSET_Y : -16.0f;
    }
    if (m_IsCrouching && IsBig()) {
        return BIG_RENDER_OFFSET_Y + (BIG_HALF_HEIGHT - CROUCH_HALF_HEIGHT);
    }
    return IsBig() ? BIG_RENDER_OFFSET_Y : 0.0f;
}

glm::vec2 Mario::GetFireballSpawnPosition() const {
    const float direction = GetFacingDirection();
    const float spawnX = m_Transform.translation.x + direction * 26.0f;
    const float spawnY = m_Transform.translation.y + (IsBig() ? 2.0f : -2.0f);
    return { spawnX, spawnY };
}

std::map<Mario::AnimState, std::unique_ptr<Animation>>& Mario::ActiveAnimations() {
    if (m_PowerState == PowerState::Fire) return m_FireAnimations;
    return (m_PowerState == PowerState::Big) ? m_BigAnimations : m_SmallAnimations;
}

const std::map<Mario::AnimState, std::unique_ptr<Animation>>& Mario::ActiveAnimations() const {
    if (m_PowerState == PowerState::Fire) return m_FireAnimations;
    return (m_PowerState == PowerState::Big) ? m_BigAnimations : m_SmallAnimations;
}

std::map<Mario::AnimState, std::unique_ptr<Animation>>& Mario::AnimationsForPowerState(PowerState state) {
    if (state == PowerState::Fire) return m_FireAnimations;
    return (state == PowerState::Big) ? m_BigAnimations : m_SmallAnimations;
}

const std::map<Mario::AnimState, std::unique_ptr<Animation>>& Mario::AnimationsForPowerState(PowerState state) const {
    if (state == PowerState::Fire) return m_FireAnimations;
    return (state == PowerState::Big) ? m_BigAnimations : m_SmallAnimations;
}

Animation& Mario::ActiveFlagAnimation() {
    return *m_FlagAnimations.at(m_PowerState);
}

const Animation& Mario::ActiveFlagAnimation() const {
    return *m_FlagAnimations.at(m_PowerState);
}

std::string Mario::CurrentAnimatedFramePath() const {
    return ActiveAnimations().at(m_AnimState)->GetCurrentFramePath();
}

std::string Mario::FramePathForState(PowerState powerState, AnimState animState) const {
    const auto& animations = AnimationsForPowerState(powerState);
    AnimState resolvedState = animState;

    if (animations.find(resolvedState) == animations.end()) {
        resolvedState = AnimState::IDLE;
    }
    if (resolvedState == AnimState::CROUCH && powerState == PowerState::Small) {
        resolvedState = AnimState::IDLE;
    }

    return animations.at(resolvedState)->GetCurrentFramePath();
}

std::string Mario::DisplayFramePathForBase(const std::string& basePath) const {
    if (m_TransformType == TransformType::SmallBigTransition) {
        const float elapsed = SMALL_BIG_TRANSITION_DURATION - m_TransformTimer;
        const int phase = static_cast<int>(std::max(0.0f, elapsed) / SMALL_BIG_TRANSITION_FRAME_INTERVAL);
        const bool showTargetFrame = (phase % 2) == 1;
        const PowerState shownPowerState = showTargetFrame ? m_TargetPowerState : m_PowerState;
        return FramePathForState(shownPowerState, m_TransformAnimState);
    }

    if (m_TransformType == TransformType::FireTransition) {
        const float elapsed = FIRE_TRANSITION_DURATION - m_TransformTimer;
        const PaletteVariant variant = FireTransitionPaletteVariant(std::max(0.0f, elapsed));
        const std::string sourcePath = FramePathForState(m_PowerState, m_TransformAnimState);
        const std::string flashingPath = ResolvePaletteFramePath(sourcePath, variant);
        const int phase = static_cast<int>(elapsed / FIRE_TRANSITION_FRAME_INTERVAL);
        const bool showFireFrame = (phase % 2) == 1;

        if (showFireFrame && m_TargetPowerState == PowerState::Fire) {
            return FramePathForState(PowerState::Fire, m_TransformAnimState);
        }

        return flashingPath;
    }

    if (m_TransformType == TransformType::StarTransition) {
        const float elapsed = STAR_TRANSITION_DURATION - m_TransformTimer;
        const PaletteVariant variant = StarTransitionPaletteVariant(std::max(0.0f, elapsed));
        const std::string sourcePath = FramePathForState(m_PowerState, m_TransformAnimState);
        if (m_PowerState == PowerState::Fire && !HasPaletteVariantForPath(sourcePath)) {
            const std::string bigPath = FramePathForState(PowerState::Big, m_TransformAnimState);
            const std::string flashingPath = ResolvePaletteFramePath(bigPath, variant);
            const int phase = static_cast<int>(elapsed / STAR_TRANSITION_FRAME_INTERVAL);
            return (phase % 2) == 0 ? sourcePath : flashingPath;
        }
        return ResolvePaletteFramePath(sourcePath, variant);
    }

    if (m_StarPowerTimer > 0.0f) {
        const float elapsed = STAR_POWER_DURATION - m_StarPowerTimer;
        const PaletteVariant variant = StarPaletteVariant(std::max(0.0f, elapsed));
        if (m_PowerState == PowerState::Fire && !HasPaletteVariantForPath(basePath)) {
            const std::string bigPath = FramePathForState(PowerState::Big, m_AnimState);
            const std::string flashingPath = ResolvePaletteFramePath(bigPath, variant);
            const int phase = static_cast<int>(elapsed / STAR_FLASH_FRAME_INTERVAL);
            return (phase % 2) == 0 ? basePath : flashingPath;
        }
        return ResolvePaletteFramePath(basePath, variant);
    }

    return basePath;
}

void Mario::ResetAnimations() {
    for (auto& entry : ActiveAnimations()) {
        if (entry.second) {
            entry.second->Reset();
        }
    }
    for (auto& entry : m_FlagAnimations) {
        if (entry.second) {
            entry.second->Reset();
        }
    }
}

void Mario::BeginTransformation(PowerState targetState, TransformType transformType) {
    if (m_IsDead || m_TransformType != TransformType::None) return;

    const float originalY = m_Transform.translation.y;
    m_TargetPowerState = targetState;
    m_TransformType = transformType;
    if (transformType == TransformType::SmallBigTransition) {
        m_TransformTimer = SMALL_BIG_TRANSITION_DURATION;
    } else if (transformType == TransformType::FireTransition) {
        m_TransformTimer = FIRE_TRANSITION_DURATION;
    } else {
        m_TransformTimer = STAR_TRANSITION_DURATION;
    }
    m_JumpTimer = 0.0f;
    m_TransformAnimState = m_AnimState;
    if (!m_OnGround) {
        m_TransformAnimState = AnimState::JUMP;
    } else if (m_IsCrouching) {
        m_TransformAnimState = IsBig() ? AnimState::CROUCH : AnimState::IDLE;
    }
    m_TransformShowBigFrame = false;
    m_StoredVelocityX = m_VelocityX;
    m_StoredVelocityY = m_VelocityY;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
    SetVisible(true);

    if (transformType == TransformType::SmallBigTransition) {
        if (m_PowerState == PowerState::Small && targetState != PowerState::Small) {
            m_Transform.translation.y += (BIG_HALF_HEIGHT - SMALL_HALF_HEIGHT);
            ClampGrowthToAvailableHeadroom(
                m_Transform.translation,
                originalY,
                { BIG_HALF_WIDTH, BIG_HALF_HEIGHT }
            );
        }
    }

    m_Image->SetImage(DisplayFramePathForBase(FramePathForState(m_PowerState, m_TransformAnimState)));
}

void Mario::SetPowerState(PowerState newState) {
    if (m_PowerState == newState) return;

    const float oldHalfHeight = GetHalfExtents().y;
    if (newState == PowerState::Small) {
        m_IsCrouching = false;
    }
    m_PowerState = newState;
    const float newHalfHeight = GetHalfExtents().y;
    m_Transform.translation.y += (newHalfHeight - oldHalfHeight);

    ResetAnimations();
    m_AnimState = AnimState::IDLE;
    m_Image->SetImage(ActiveAnimations().at(m_AnimState)->GetCurrentFramePath());
}

void Mario::ActivateStarPower() {
    m_StarPowerTimer = STAR_POWER_DURATION;
    SetVisible(true);
}

void Mario::RestoreStoredMotion() {
    m_VelocityX = m_StoredVelocityX;
    m_VelocityY = m_StoredVelocityY;
}

void Mario::PowerUp(LootType type) {
    if (type == LootType::Coin) {
        return;
    }

    if (type == LootType::Star) {
        BeginTransformation(m_PowerState, TransformType::StarTransition);
        return;
    }

    if (type == LootType::GreenMushroom) {
        if (m_PowerState == PowerState::Small) {
            BeginTransformation(PowerState::Big, TransformType::SmallBigTransition);
        }
        return;
    }

    if (type == LootType::FireFlower) {
        if (m_PowerState != PowerState::Fire) {
            BeginTransformation(PowerState::Fire, TransformType::FireTransition);
        }
        return;
    }

    if (m_PowerState == PowerState::Small) {
        BeginTransformation(PowerState::Big, TransformType::SmallBigTransition);
    }
}

void Mario::TakeEnemyHit() {
    if (m_IsDead || IsInvulnerable() || m_PowerDownLockTimer > 0.0f ||
        m_TransformType != TransformType::None) return;

    if (m_PowerState == PowerState::Fire) {
        SetPowerState(PowerState::Small);
        m_TransformType = TransformType::None;
        m_TargetPowerState = PowerState::Small;
        m_TransformTimer = 0.0f;
        m_TransformShowBigFrame = false;
        m_PowerDownLockTimer = 0.35f;
        m_InvulnerabilityTimer = INVULNERABILITY_DURATION;
        SetVisible(true);
        return;
    }

    if (m_PowerState == PowerState::Big) {
        BeginTransformation(PowerState::Small, TransformType::SmallBigTransition);
        m_PowerDownLockTimer = 0.35f;
        m_InvulnerabilityTimer = INVULNERABILITY_DURATION;
        SetVisible(true);
        return;
    }

    Die();
}

void Mario::Update() {
    float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);

    if (m_GoalSequenceState != GoalSequenceState::None &&
        m_GoalSequenceState != GoalSequenceState::Finished) {
        const float standingY = m_GoalGroundY + GetHalfExtents().y;
        const float poleExitX = m_GoalPoleX + POLE_EXIT_OFFSET_X;
        m_IsCrouching = false;
        m_VelocityX = 0.0f;
        m_VelocityY = 0.0f;
        m_OnGround = false;
        if (m_GoalSequenceState == GoalSequenceState::SlideDownFlag) {
            m_Transform.scale.x = std::abs(m_Transform.scale.x);
        } else {
            m_Transform.scale.x = -std::abs(m_Transform.scale.x);
        }
        SetVisible(true);

        if (m_GoalSequenceState == GoalSequenceState::SlideDownFlag) {
            m_Transform.translation.x = m_GoalFlagX + FLAG_SLIDE_OFFSET_X;
            m_Transform.translation.y = std::max(m_GoalFlagBottomY, m_Transform.translation.y - GOAL_SLIDE_SPEED * dt);
            if (m_Transform.translation.y <= m_GoalFlagBottomY + 0.5f) {
                m_Transform.translation.y = m_GoalFlagBottomY;
                m_GoalSequenceState = GoalSequenceState::CrossPole;
                ActiveAnimations().at(AnimState::WALK)->Reset();
            }
        } else if (m_GoalSequenceState == GoalSequenceState::CrossPole) {
            m_Transform.translation.y = m_GoalFlagBottomY;
            m_Transform.translation.x = std::min(poleExitX, m_Transform.translation.x + GOAL_CROSS_POLE_SPEED * dt);
            if (m_Transform.translation.x >= poleExitX - 0.5f) {
                m_Transform.translation.x = poleExitX;
                m_GoalSequenceState = GoalSequenceState::DropFromPole;
            }
        } else if (m_GoalSequenceState == GoalSequenceState::DropFromPole) {
            m_Transform.translation.x = poleExitX;
            m_Transform.translation.y = std::max(standingY, m_Transform.translation.y - GOAL_DROP_SPEED * dt);
            if (m_Transform.translation.y <= standingY + 0.5f) {
                m_Transform.translation.y = standingY;
                m_GoalSequenceState = GoalSequenceState::Finished;
                m_OnGround = true;
                m_Transform.scale.x = -std::abs(m_Transform.scale.x);
            }
        }

        HandleAnimation(dt);
        return;
    }

    if (m_IsDead) {
        m_VelocityY += m_Gravity * dt;
        m_Transform.translation.y += m_VelocityY * dt;
        m_RespawnTimer -= dt;
        if (m_RespawnTimer <= 0.0f) {
            m_IsDead = false;
            m_VelocityX = 0.0f;
            m_VelocityY = 0.0f;
            m_OnGround = false;
            m_IsCrouching = false;
            m_JumpTimer = 0.0f;
            m_InvulnerabilityTimer = 0.0f;
            m_StarPowerTimer = 0.0f;
            m_PowerDownLockTimer = 0.0f;
            m_PowerState = PowerState::Small;
            m_AnimState = AnimState::IDLE;
            ResetAnimations();
            SetVisible(true);
            m_Transform.translation = m_SpawnPosition;
            m_Image->SetImage(ActiveAnimations().at(AnimState::IDLE)->GetCurrentFramePath());
        }
        return;
    }

    if (m_TransformType == TransformType::FireTransition) {
        m_TransformTimer = std::max(0.0f, m_TransformTimer - dt);
        m_Image->SetImage(DisplayFramePathForBase(FramePathForState(m_PowerState, m_TransformAnimState)));

        if (m_TransformTimer <= 0.0f) {
            m_TransformType = TransformType::None;
            SetPowerState(m_TargetPowerState);
            RestoreStoredMotion();
        }
        return;
    }

    if (m_TransformType == TransformType::SmallBigTransition) {
        m_TransformTimer = std::max(0.0f, m_TransformTimer - dt);
        const int flashIndex = static_cast<int>(m_TransformTimer / SMALL_BIG_TRANSITION_FRAME_INTERVAL);
        m_TransformShowBigFrame = (flashIndex % 2) == 0;
        m_Image->SetImage(DisplayFramePathForBase(FramePathForState(m_PowerState, m_TransformAnimState)));
        if (m_TransformTimer <= 0.0f) {
            m_TransformType = TransformType::None;
            m_TransformShowBigFrame = true;
            SetPowerState(m_TargetPowerState);
            RestoreStoredMotion();
        }
        return;
    }

    if (m_TransformType == TransformType::StarTransition) {
        m_TransformTimer = std::max(0.0f, m_TransformTimer - dt);
        m_Image->SetImage(DisplayFramePathForBase(FramePathForState(m_PowerState, m_TransformAnimState)));
        if (m_TransformTimer <= 0.0f) {
            m_TransformType = TransformType::None;
            ActivateStarPower();
            RestoreStoredMotion();
        }
        return;
    }

    SnapUpOutOfGround(m_Transform.translation, GetHalfExtents());

    if (m_PowerDownLockTimer > 0.0f) {
        m_PowerDownLockTimer = std::max(0.0f, m_PowerDownLockTimer - dt);
    }

    if (m_InvulnerabilityTimer > 0.0f) {
        m_InvulnerabilityTimer = std::max(0.0f, m_InvulnerabilityTimer - dt);
    }

    if (m_StarPowerTimer > 0.0f) {
        m_StarPowerTimer = std::max(0.0f, m_StarPowerTimer - dt);
    }

    if (m_InvulnerabilityTimer > 0.0f) {
        const bool blinkVisible =
            static_cast<int>(m_InvulnerabilityTimer * 12.0f) % 2 == 0;
        SetVisible(blinkVisible);
    } else {
        SetVisible(true);
    }

    bool moveLeft = Util::Input::IsKeyPressed(Util::Keycode::A);
    bool moveRight = Util::Input::IsKeyPressed(Util::Keycode::D);
    if (m_TransformType == TransformType::SmallBigTransition) {
        moveLeft = false;
        moveRight = false;
    }
    const bool wasCrouching = m_IsCrouching;
    const bool wantsCrouch = Util::Input::IsKeyPressed(Util::Keycode::S);
    if (wantsCrouch && m_OnGround) {
        m_IsCrouching = true;
    } else if (IsBig() && m_IsCrouching && g_MapManager) {
            float tileSize = g_MapManager->GetTileSize();
            float mapLeft = -(g_MapManager->GetWidth() * tileSize) / 2.0f;
            float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
            const float eps = 0.001f;

            float standingTop = m_Transform.translation.y + BIG_HALF_HEIGHT;
            float leftX = m_Transform.translation.x - BIG_HALF_WIDTH;
            float rightX = m_Transform.translation.x + BIG_HALF_WIDTH;
            int leftGridX = std::clamp(static_cast<int>(std::floor((leftX - mapLeft + eps) / tileSize)), 0, std::max(0, g_MapManager->GetWidth() - 1));
            int rightGridX = std::clamp(static_cast<int>(std::floor((rightX - mapLeft - eps) / tileSize)), 0, std::max(0, g_MapManager->GetWidth() - 1));
            int topGridY = std::clamp(static_cast<int>(std::floor((mapTop - standingTop + eps) / tileSize)), 0, std::max(0, g_MapManager->GetHeight() - 1));

            bool blocked = false;
            for (int gx = leftGridX; gx <= rightGridX; ++gx) {
                if (g_MapManager->IsSolidAt(gx, topGridY)) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                m_IsCrouching = false;
            }
    } else {
        m_IsCrouching = false;
    }
    if (IsBig() && wasCrouching != m_IsCrouching) {
        const float delta = BIG_HALF_HEIGHT - CROUCH_HALF_HEIGHT;
        m_Transform.translation.y += m_IsCrouching ? -delta : delta;
    }

    bool isMoving = false;
    if (!m_IsCrouching) {
        if (moveLeft && !moveRight) {
            m_VelocityX -= m_Acceleration * dt;
            isMoving = true;
            m_Transform.scale.x = -std::abs(m_Transform.scale.x);
        }
        else if (moveRight && !moveLeft) {
            m_VelocityX += m_Acceleration * dt;
            isMoving = true;
            m_Transform.scale.x = std::abs(m_Transform.scale.x);
        }
    }

    if (!isMoving && m_OnGround) {
        if (m_VelocityX > 0) m_VelocityX = std::max(0.0f, m_VelocityX - m_Friction * dt);
        else if (m_VelocityX < 0) m_VelocityX = std::min(0.0f, m_VelocityX + m_Friction * dt);
    }

    m_VelocityX = std::clamp(m_VelocityX, -m_MaxSpeed, m_MaxSpeed);

    if (m_TransformType == TransformType::None &&
        !m_IsCrouching && Util::Input::IsKeyDown(Util::Keycode::SPACE) && m_OnGround) {
        m_VelocityY = m_JumpForce;
        m_OnGround = false;
        m_JumpTimer = m_MaxJumpTime;
    }

    if (m_TransformType == TransformType::None &&
        Util::Input::IsKeyPressed(Util::Keycode::SPACE) && m_JumpTimer > 0.0f) {
        m_VelocityY = m_JumpForce;
        m_JumpTimer -= dt;
    }
    else {
        m_JumpTimer = 0.0f;
    }

    m_VelocityY += m_Gravity * dt;

    float moveX = m_VelocityX * dt;
    float moveY = m_VelocityY * dt;

    // axis-separated collision resolution:
    // - first resolve horizontal movement (with current Y)
    // - then resolve vertical movement (with resolved X)

    m_OnGround = false;

    // use map tile size and geometry from map manager if available
    float tileSize = 48.0f;
    int mapWidth = 0;
    int mapHeight = 0;
    float mapLeft = 0.0f;
    float mapTop = 0.0f;
    if (g_MapManager) {
        tileSize = g_MapManager->GetTileSize();
        mapWidth = g_MapManager->GetWidth();
        mapHeight = g_MapManager->GetHeight();
        mapLeft = -(mapWidth * tileSize) / 2.0f;
        mapTop = (mapHeight * tileSize) / 2.0f;
    }

    // player extents (half sizes) in world units
    glm::vec2 halfExtents = GetHalfExtents();
    float halfWidth = halfExtents.x;
    float halfHeight = halfExtents.y;

    const float eps = 0.001f;

    float curX = m_Transform.translation.x;
    float curY = m_Transform.translation.y;

    float candidateX = curX;
    float candidateY = curY;

    // --- HORIZONTAL ---
    if (g_MapManager && std::abs(moveX) > 0.0f) {
        candidateX = curX + moveX;
        // sample vertically between top and bottom
        float topY = curY + halfHeight - eps;
        float bottomY = curY - halfHeight + eps;

        int topGridY = static_cast<int>(std::floor((mapTop - topY) / tileSize));
        int bottomGridY = static_cast<int>(std::floor((mapTop - bottomY) / tileSize));

        topGridY = std::max(0, std::min(mapHeight - 1, topGridY));
        bottomGridY = std::max(0, std::min(mapHeight - 1, bottomGridY));

        if (moveX > 0.0f) {
            float rightEdge = candidateX + halfWidth;
            int gridX = static_cast<int>(std::floor((rightEdge - mapLeft - eps) / tileSize));
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                if (g_MapManager->IsSolidAt(gridX, gy)) {
                    // collide with tile at (gridX, gy)
                    float tileLeft = mapLeft + gridX * tileSize;
                    candidateX = tileLeft - halfWidth - eps;
                    m_VelocityX = 0.0f;
                    break;
                }
            }
        }
        else { // moving left
            float leftEdge = candidateX - halfWidth;
            int gridX = static_cast<int>(std::floor((leftEdge - mapLeft + eps) / tileSize));
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                if (g_MapManager->IsSolidAt(gridX, gy)) {
                    float tileRight = mapLeft + (gridX + 1) * tileSize;
                    candidateX = tileRight + halfWidth + eps;
                    m_VelocityX = 0.0f;
                    break;
                }
            }
        }
    }
    else {
        candidateX = curX + moveX;
    }

    // --- VERTICAL ---
    if (g_MapManager && std::abs(moveY) > 0.0f) {
        candidateY = curY + moveY;

        float leftX = candidateX - halfWidth;
        float rightX = candidateX + halfWidth;

        int leftGridX = static_cast<int>(std::floor((leftX - mapLeft + eps) / tileSize));
        int rightGridX = static_cast<int>(std::floor((rightX - mapLeft - eps) / tileSize));

        leftGridX = std::max(0, std::min(mapWidth - 1, leftGridX));
        rightGridX = std::max(0, std::min(mapWidth - 1, rightGridX));

        if (moveY > 0.0f) { // moving up (jump)
            float topEdge = candidateY + halfHeight;
            int gridY = static_cast<int>(std::floor((mapTop - topEdge + eps) / tileSize));
            gridY = std::max(0, std::min(mapHeight - 1, gridY));
            for (int gx = leftGridX; gx <= rightGridX; ++gx) {
                Cell hitCell = g_MapManager->GetCell(gx, gridY);
                if (MapManager::IsSolidCell(hitCell)) {
                    if (hitCell == Cell::QuestionBlock) {
                        g_MapManager->HitQuestionBlock(gx, gridY, IsBig());
                    } else if (hitCell == Cell::Brick && IsBig() && !m_IsCrouching) {
                        g_MapManager->BreakBrick(gx, gridY);
                    }
                    // collide with ceiling of tile at (gx, gridY)
                    float tileBottom = mapTop - (gridY + 1) * tileSize;
                    candidateY = tileBottom - halfHeight - eps;
                    m_VelocityY = 0.0f;
                    m_JumpTimer = 0.0f;
                    break;
                }
            }
        }
        else { // moving down (fall)
            float bottomEdge = candidateY - halfHeight;
            int gridY = static_cast<int>(std::floor((mapTop - bottomEdge + eps) / tileSize));
            gridY = std::max(0, std::min(mapHeight - 1, gridY));
            for (int gx = leftGridX; gx <= rightGridX; ++gx) {
                if (g_MapManager->IsSolidAt(gx, gridY)) {
                    // collide with top of tile at (gx, gridY)
                    float tileTop = mapTop - gridY * tileSize;
                    candidateY = tileTop + halfHeight;
                    m_VelocityY = 0.0f;
                    m_OnGround = true;
                    break;
                }
            }
        }
    }
    else {
        candidateY = curY + moveY;
    }

    // Apply resolved candidate position
    m_Transform.translation.x = candidateX;
    m_Transform.translation.y = candidateY;

    if (g_MapManager) {
        float tileSize = g_MapManager->GetTileSize();
        float mapLeft = -(g_MapManager->GetWidth() * tileSize) / 2.0f;
        float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
        const float eps = 0.001f;
        glm::vec2 coinHalf = halfExtents;
        int leftGridX = std::clamp(static_cast<int>(std::floor((m_Transform.translation.x - coinHalf.x - mapLeft + eps) / tileSize)), 0, std::max(0, g_MapManager->GetWidth() - 1));
        int rightGridX = std::clamp(static_cast<int>(std::floor((m_Transform.translation.x + coinHalf.x - mapLeft - eps) / tileSize)), 0, std::max(0, g_MapManager->GetWidth() - 1));
        int topGridY = std::clamp(static_cast<int>(std::floor((mapTop - (m_Transform.translation.y + coinHalf.y - eps)) / tileSize)), 0, std::max(0, g_MapManager->GetHeight() - 1));
        int bottomGridY = std::clamp(static_cast<int>(std::floor((mapTop - (m_Transform.translation.y - coinHalf.y + eps)) / tileSize)), 0, std::max(0, g_MapManager->GetHeight() - 1));

        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                g_MapManager->CollectCoin(gx, gy);
            }
        }

        const float mapBottom = -(g_MapManager->GetHeight() * tileSize) / 2.0f;
        if (!m_IsDead && (m_Transform.translation.y + halfHeight) < mapBottom) {
            Die();
            return;
        }
    }

    HandleAnimation(dt);
}

void Mario::Die() {
    if (m_IsDead) return;
    m_IsDead = true;
    m_RespawnTimer = 1.25f;
    m_VelocityX = 0.0f;
    m_VelocityY = 900.0f;
    m_IsCrouching = false;
    m_OnGround = false;
    SetVisible(true);
    m_Image->SetImage(m_DeathFramePath);
}

void Mario::BounceAfterStomp() {
    m_VelocityY = m_JumpForce * 0.6f;
    m_OnGround = false;
    m_JumpTimer = 0.0f;
}

void Mario::StartGoalSequence(float poleX, float flagX, float flagBottomY, float groundY, float slideStartY) {
    if (m_IsDead || m_GoalSequenceState != GoalSequenceState::None) return;

    m_GoalPoleX = poleX;
    m_GoalFlagX = flagX;
    m_GoalFlagBottomY = flagBottomY;
    m_GoalGroundY = groundY;
    m_GoalSlideStartY = slideStartY;
    m_TransformType = TransformType::None;
    m_TransformTimer = 0.0f;
    m_TargetPowerState = m_PowerState;
    m_InvulnerabilityTimer = 0.0f;
    m_PowerDownLockTimer = 0.0f;
    m_IsCrouching = false;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
    m_JumpTimer = 0.0f;
    m_OnGround = false;
    m_GoalSequenceState = GoalSequenceState::SlideDownFlag;
    m_Transform.scale.x = std::abs(m_Transform.scale.x);
    m_Transform.translation.x = m_GoalFlagX + FLAG_SLIDE_OFFSET_X;
    const float standingY = m_GoalGroundY + GetHalfExtents().y;
    m_Transform.translation.y = std::max(m_GoalSlideStartY, standingY);
    ActiveFlagAnimation().Reset();
    SetVisible(true);
}

void Mario::HandleAnimation(float dt) {
    if (m_IsDead) {
        m_Image->SetImage(m_DeathFramePath);
        return;
    }

    if (m_GoalSequenceState != GoalSequenceState::None &&
        m_GoalSequenceState != GoalSequenceState::Finished) {
        if (m_GoalSequenceState == GoalSequenceState::SlideDownFlag) {
            Animation& flagAnimation = ActiveFlagAnimation();
            flagAnimation.Update(dt);
            m_Image->SetImage(flagAnimation.GetCurrentFramePath());
        } else if (m_GoalSequenceState == GoalSequenceState::DropFromPole) {
            m_AnimState = AnimState::JUMP;
            m_Image->SetImage(ActiveAnimations().at(m_AnimState)->GetCurrentFramePath());
        } else if (m_GoalSequenceState == GoalSequenceState::Finished) {
            m_AnimState = AnimState::IDLE;
            m_Image->SetImage(ActiveAnimations().at(m_AnimState)->GetCurrentFramePath());
        } else {
            m_AnimState = AnimState::WALK;
            auto& animations = ActiveAnimations();
            animations[m_AnimState]->Update(dt);
            m_Image->SetImage(animations[m_AnimState]->GetCurrentFramePath());
        }
        return;
    }

    if (m_TransformType != TransformType::None) {
        if (m_TransformType == TransformType::SmallBigTransition) {
            const std::string path = m_TransformShowBigFrame
                ? m_BigAnimations.at(AnimState::IDLE)->GetCurrentFramePath()
                : m_SmallAnimations.at(AnimState::IDLE)->GetCurrentFramePath();
            m_Image->SetImage(path);
        } else {
            m_Image->SetImage(DisplayFramePathForBase(CurrentAnimatedFramePath()));
        }
        return;
    }

    AnimState lastState = m_AnimState;

    if (!m_OnGround) m_AnimState = AnimState::JUMP;
    else if (m_IsCrouching) m_AnimState = AnimState::CROUCH;
    else if (std::abs(m_VelocityX) > 20.0f) {
        if (m_VelocityX > 100.0f && Util::Input::IsKeyPressed(Util::Keycode::A))
            m_AnimState = AnimState::BRAKE;
        else if (m_VelocityX < -100.0f && Util::Input::IsKeyPressed(Util::Keycode::D))
            m_AnimState = AnimState::BRAKE;
        else
            m_AnimState = AnimState::WALK;
    }
    else {
        m_AnimState = AnimState::IDLE;
    }

    auto& animations = ActiveAnimations();

    if (lastState != m_AnimState) {
        animations[m_AnimState]->Reset();
    }

    animations[m_AnimState]->Update(dt);

    m_Image->SetImage(DisplayFramePathForBase(animations[m_AnimState]->GetCurrentFramePath()));
}
