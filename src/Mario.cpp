#include "Mario.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include "AssetPaths.hpp"
#include <algorithm>
#include <cmath>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {
constexpr float GOAL_SLIDE_SPEED = 220.0f;
constexpr float GOAL_WALK_SPEED = 110.0f;
constexpr float GOAL_ENTER_DURATION = 0.4f;
constexpr float POLE_GRAB_OFFSET_X = 18.0f;

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
      m_Acceleration(1500.0f),
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
        AssetPaths::Image("character/mario_fire_idle.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::WALK] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/mario_fire_walk1.png"),
            AssetPaths::Image("character/mario_fire_walk2.png"),
            AssetPaths::Image("character/mario_fire_walk3.png")
    }, 0.07f
    );

    m_FireAnimations[AnimState::JUMP] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/mario_fire_jump.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::BRAKE] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/mario_fire_brake.png")
    }, 1.0f
    );

    m_FireAnimations[AnimState::CROUCH] = std::make_unique<Animation>(
        std::vector<std::string>{
        AssetPaths::Image("character/mario_fire_crouch.png")
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
            AssetPaths::Image("character/mario_fire_flag1.png"),
            AssetPaths::Image("character/mario_fire_flag2.png")
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
    if (IsBig() || m_TransformType == TransformType::SmallBigTransition) {
        return { BIG_HALF_WIDTH, BIG_HALF_HEIGHT };
    }
    return { SMALL_HALF_WIDTH, SMALL_HALF_HEIGHT };
}

float Mario::GetRenderOffsetY() const {
    if (m_TransformType == TransformType::SmallBigTransition) {
        return m_TransformShowBigFrame ? 5.0f : -16.0f;
    }
    if (m_IsCrouching) {
        if (m_PowerState == PowerState::Fire) return 16.0f;
        if (m_PowerState == PowerState::Big) return 9.0f;
    }
    return IsBig() ? 5.0f : 0.0f;
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

Animation& Mario::ActiveFlagAnimation() {
    return *m_FlagAnimations.at(m_PowerState);
}

const Animation& Mario::ActiveFlagAnimation() const {
    return *m_FlagAnimations.at(m_PowerState);
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
    m_TransformTimer = (transformType == TransformType::SmallBigTransition) ? 0.55f : 0.75f;
    m_JumpTimer = 0.0f;
    m_IsCrouching = false;
    m_TransformShowBigFrame = false;
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
        m_Image->SetImage(m_SmallAnimations.at(AnimState::IDLE)->GetCurrentFramePath());
    } else if (m_TransformAnimation) {
        m_VelocityX = 0.0f;
        m_VelocityY = 0.0f;
        m_OnGround = true;
        m_TransformAnimation->Reset();
        m_Image->SetImage(m_TransformAnimation->GetCurrentFramePath());
    }
}

void Mario::SetPowerState(PowerState newState) {
    if (m_PowerState == newState) return;

    const float oldHalfHeight = (m_IsCrouching && IsBig()) ? SMALL_HALF_HEIGHT : GetHalfExtents().y;
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

void Mario::PowerUp(LootType type) {
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

    if (m_GoalSequenceState != GoalSequenceState::None) {
        const float standingY = m_GoalGroundY + GetHalfExtents().y;
        m_IsCrouching = false;
        m_VelocityX = 0.0f;
        m_VelocityY = 0.0f;
        m_OnGround = false;
        m_Transform.scale.x = std::abs(m_Transform.scale.x);
        SetVisible(true);

        if (m_GoalSequenceState == GoalSequenceState::SlideDownFlag) {
            m_Transform.translation.x = m_GoalPoleX + POLE_GRAB_OFFSET_X;
            m_Transform.translation.y = std::max(standingY, m_Transform.translation.y - GOAL_SLIDE_SPEED * dt);
            if (m_Transform.translation.y <= standingY + 0.5f) {
                m_Transform.translation.y = standingY;
                m_GoalSequenceState = GoalSequenceState::WalkToCastle;
                ActiveAnimations().at(AnimState::WALK)->Reset();
            }
        } else if (m_GoalSequenceState == GoalSequenceState::WalkToCastle) {
            m_OnGround = true;
            m_Transform.translation.y = standingY;
            m_Transform.translation.x += GOAL_WALK_SPEED * dt;
            if (m_Transform.translation.x >= m_CastleDoorX) {
                m_Transform.translation.x = m_CastleDoorX;
                m_GoalSequenceState = GoalSequenceState::EnterCastle;
                m_GoalEnterTimer = GOAL_ENTER_DURATION;
            }
        } else if (m_GoalSequenceState == GoalSequenceState::EnterCastle) {
            m_OnGround = true;
            m_Transform.translation.y = standingY;
            m_Transform.translation.x += GOAL_WALK_SPEED * dt;
            m_GoalEnterTimer = std::max(0.0f, m_GoalEnterTimer - dt);
            if (m_GoalEnterTimer <= 0.0f) {
                SetVisible(false);
                m_GoalSequenceState = GoalSequenceState::Finished;
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
        if (m_TransformAnimation) {
            m_TransformAnimation->Update(dt);
            m_Image->SetImage(m_TransformAnimation->GetCurrentFramePath());
        }

        if (m_TransformTimer <= 0.0f) {
            m_TransformType = TransformType::None;
            SetPowerState(m_TargetPowerState);
        }
        return;
    }

    if (m_TransformType == TransformType::SmallBigTransition) {
        m_TransformTimer = std::max(0.0f, m_TransformTimer - dt);
        const int flashIndex = static_cast<int>(m_TransformTimer / 0.08f);
        m_TransformShowBigFrame = (flashIndex % 2) == 0;
        if (m_TransformTimer <= 0.0f) {
            m_TransformType = TransformType::None;
            m_TransformShowBigFrame = true;
            SetPowerState(m_TargetPowerState);
        }
    }

    glm::vec2 snapHalfExtents = GetHalfExtents();
    if (m_IsCrouching && IsBig()) {
        snapHalfExtents.y = SMALL_HALF_HEIGHT;
    }
    SnapUpOutOfGround(m_Transform.translation, snapHalfExtents);

    if (m_PowerDownLockTimer > 0.0f) {
        m_PowerDownLockTimer = std::max(0.0f, m_PowerDownLockTimer - dt);
    }

    if (m_InvulnerabilityTimer > 0.0f) {
        m_InvulnerabilityTimer = std::max(0.0f, m_InvulnerabilityTimer - dt);
        const bool blinkVisible =
            m_InvulnerabilityTimer <= 0.0f ||
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
        const float delta = BIG_HALF_HEIGHT - SMALL_HALF_HEIGHT;
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
    if (m_IsCrouching && IsBig()) {
        halfExtents.y = SMALL_HALF_HEIGHT;
    }
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
                        g_MapManager->HitQuestionBlock(gx, gridY);
                    } else if (hitCell == Cell::Brick && IsBig() && !m_IsCrouching) {
                        g_MapManager->BreakBrick(gx, gridY);
                    }
                    // collide with ceiling of tile at (gx, gridY)
                    float tileBottom = mapTop - (gridY + 1) * tileSize;
                    candidateY = tileBottom - halfHeight - eps;
                    m_VelocityY = 0.0f;
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

void Mario::StartGoalSequence(float poleX, float groundY, float castleDoorX, float slideStartY) {
    if (m_IsDead || m_GoalSequenceState != GoalSequenceState::None) return;

    m_GoalPoleX = poleX;
    m_GoalGroundY = groundY;
    m_CastleDoorX = castleDoorX;
    m_GoalSlideStartY = slideStartY;
    m_GoalEnterTimer = 0.0f;
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
    m_Transform.translation.x = m_GoalPoleX + POLE_GRAB_OFFSET_X;
    m_Transform.translation.y = std::max(
        m_GoalSlideStartY,
        m_GoalGroundY + GetHalfExtents().y
    );
    ActiveFlagAnimation().Reset();
    SetVisible(true);
}

void Mario::HandleAnimation(float dt) {
    if (m_IsDead) {
        m_Image->SetImage(m_DeathFramePath);
        return;
    }

    if (m_GoalSequenceState != GoalSequenceState::None) {
        if (m_GoalSequenceState == GoalSequenceState::SlideDownFlag) {
            Animation& flagAnimation = ActiveFlagAnimation();
            flagAnimation.Update(dt);
            m_Image->SetImage(flagAnimation.GetCurrentFramePath());
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
        } else if (m_TransformAnimation) {
            m_Image->SetImage(m_TransformAnimation->GetCurrentFramePath());
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

    m_Image->SetImage(animations[m_AnimState]->GetCurrentFramePath());
}
