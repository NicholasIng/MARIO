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

    m_Image = std::make_shared<Util::Image>(
        ActiveAnimations().at(AnimState::IDLE)->GetCurrentFramePath()
    );
    SetDrawable(m_Image);

    m_Transform.translation = m_SpawnPosition;
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 10.0f;
}

glm::vec2 Mario::GetHalfExtents() const {
    if (m_PowerState == PowerState::Big) {
        return { BIG_HALF_WIDTH, BIG_HALF_HEIGHT };
    }
    return { SMALL_HALF_WIDTH, SMALL_HALF_HEIGHT };
}

std::map<Mario::AnimState, std::unique_ptr<Animation>>& Mario::ActiveAnimations() {
    return (m_PowerState == PowerState::Big) ? m_BigAnimations : m_SmallAnimations;
}

const std::map<Mario::AnimState, std::unique_ptr<Animation>>& Mario::ActiveAnimations() const {
    return (m_PowerState == PowerState::Big) ? m_BigAnimations : m_SmallAnimations;
}

void Mario::ResetAnimations() {
    for (auto& entry : ActiveAnimations()) {
        if (entry.second) {
            entry.second->Reset();
        }
    }
}

void Mario::SetPowerState(PowerState newState) {
    if (m_PowerState == newState) return;

    const float oldHalfHeight = (m_IsCrouching && m_PowerState == PowerState::Big) ? SMALL_HALF_HEIGHT : GetHalfExtents().y;
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

void Mario::PowerUp() {
    SetPowerState(PowerState::Big);
}

void Mario::TakeEnemyHit() {
    if (m_IsDead || IsInvulnerable()) return;

    if (IsBig()) {
        SetPowerState(PowerState::Small);
        m_InvulnerabilityTimer = INVULNERABILITY_DURATION;
        SetVisible(true);
        return;
    }

    Die();
}

void Mario::Update() {
    float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);

    if (m_IsDead) {
        m_RespawnTimer -= dt;
        if (m_RespawnTimer <= 0.0f) {
            m_IsDead = false;
            m_VelocityX = 0.0f;
            m_VelocityY = 0.0f;
            m_OnGround = false;
            m_JumpTimer = 0.0f;
            m_InvulnerabilityTimer = 0.0f;
            m_PowerState = PowerState::Small;
            m_AnimState = AnimState::IDLE;
            ResetAnimations();
            SetVisible(true);
            m_Transform.translation = m_SpawnPosition;
            m_Image->SetImage(ActiveAnimations().at(AnimState::IDLE)->GetCurrentFramePath());
        }
        return;
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
    const bool wasCrouching = m_IsCrouching;
    const bool wantsCrouch = Util::Input::IsKeyPressed(Util::Keycode::S);
    if (IsBig()) {
        if (wantsCrouch && m_OnGround) {
            m_IsCrouching = true;
        } else if (m_IsCrouching && g_MapManager) {
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

    if (!m_IsCrouching && Util::Input::IsKeyDown(Util::Keycode::SPACE) && m_OnGround) {
        m_VelocityY = m_JumpForce;
        m_OnGround = false;
        m_JumpTimer = m_MaxJumpTime;
    }

    if (Util::Input::IsKeyPressed(Util::Keycode::SPACE) && m_JumpTimer > 0.0f) {
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
                        g_MapManager->ClearTile(gx, gridY);
                        continue;
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
            int gridY = static_cast<int>(std::floor((mapTop - bottomEdge - eps) / tileSize));
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
    m_RespawnTimer = 1.0f;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
    SetVisible(true);
}

void Mario::BounceAfterStomp() {
    m_VelocityY = m_JumpForce * 0.6f;
    m_OnGround = false;
    m_JumpTimer = 0.0f;
}

void Mario::HandleAnimation(float dt) {
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
