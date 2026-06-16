#include "Mario.hpp"
#include "MarioDetail.hpp"
#include "GameImage.hpp"
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

using namespace MarioDetail;

Mario::Mario()
    : m_VelocityX(0.0f),
      m_VelocityY(0.0f),
      m_Acceleration(SMB3_GROUND_ACCEL),
      m_MaxSpeed(SMB3_RUN_SPEED),
      m_Friction(SMB3_RELEASE_FRICTION),
      m_Gravity(-2250.0f),
      m_JumpForce(750.0f),
      m_OnGround(false),
      m_JumpTimer(0.0f),
      m_MaxJumpTime(0.18f) {

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

    m_Image = std::make_shared<GameImage>(
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
