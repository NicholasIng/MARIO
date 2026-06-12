#include "Mario.hpp"
#include "MarioDetail.hpp"
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

using namespace MarioDetail;

void Mario::Update() {
    float dt = std::clamp(Util::Time::GetDeltaTimeMs() / 1000.0f, 0.001f, 1.0f / 30.0f);

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
        m_RespawnTimer = std::max(0.0f, m_RespawnTimer - dt);
        if (m_RespawnTimer <= 0.0f) {
            m_DeathFinished = true;
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

    if (m_VelocityY <= 0.0f) {
        ResolveMinorGroundPenetration(m_Transform.translation, GetHalfExtents());
    }

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
    if (m_GoalWalkActive && !m_GoalWalkReached) {
        moveLeft = false;
        moveRight = m_Transform.translation.x < (m_GoalWalkTargetX - 0.5f);
        if (!moveRight) {
            m_GoalWalkActive = false;
            m_GoalWalkReached = true;
            m_VelocityX = 0.0f;
        }
    }
    if (m_TransformType == TransformType::SmallBigTransition) {
        moveLeft = false;
        moveRight = false;
    }
    const bool wasCrouching = m_IsCrouching;
    const bool wantsCrouch =
        !(m_GoalWalkActive && !m_GoalWalkReached) &&
        Util::Input::IsKeyPressed(Util::Keycode::S);
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

    const float inputDirection =
        (!m_IsCrouching && moveLeft && !moveRight) ? -1.0f :
        (!m_IsCrouching && moveRight && !moveLeft) ? 1.0f :
        0.0f;
    const bool hasDirectionalInput = inputDirection != 0.0f;
    const bool starSpeedActive = HasStarPower();
    const float activeWalkSpeed = starSpeedActive ? SMB3_WALK_SPEED + 20.0f : SMB3_WALK_SPEED;
    const float activeRunSpeed = starSpeedActive ? SMB3_STAR_RUN_SPEED : m_MaxSpeed;
    const float activeGroundAccel = starSpeedActive ? SMB3_STAR_ACCEL : m_Acceleration;
    const float activeGroundRunAccel = starSpeedActive ? (SMB3_GROUND_RUN_ACCEL + 220.0f) : SMB3_GROUND_RUN_ACCEL;
    const float activeAirAccel = starSpeedActive ? (SMB3_AIR_ACCEL + 140.0f) : SMB3_AIR_ACCEL;
    const float activeFriction = starSpeedActive ? (m_Friction + 180.0f) : m_Friction;
    const float activeTurnBrake = starSpeedActive ? (SMB3_TURN_BRAKE_DECEL + 320.0f) : SMB3_TURN_BRAKE_DECEL;
    m_IsBraking = false;

    if (hasDirectionalInput) {
        const bool turningAgainstMomentum = (m_VelocityX * inputDirection) < -1.0f;
        const float currentAbsSpeed = std::abs(m_VelocityX);
        const float acceleration =
            m_OnGround
                ? (currentAbsSpeed < activeWalkSpeed ? activeGroundAccel : activeGroundRunAccel)
                : activeAirAccel;

        if (turningAgainstMomentum && m_OnGround) {
            m_IsBraking = currentAbsSpeed > 10.0f;
            m_VelocityX += inputDirection * activeTurnBrake * dt;
        } else {
            m_VelocityX += inputDirection * acceleration * dt;
        }

        if (!m_IsBraking) {
            m_Transform.scale.x =
                (inputDirection < 0.0f) ? -std::abs(m_Transform.scale.x) : std::abs(m_Transform.scale.x);
        }
    } else if (m_OnGround) {
        if (m_VelocityX > 0.0f) m_VelocityX = std::max(0.0f, m_VelocityX - activeFriction * dt);
        else if (m_VelocityX < 0.0f) m_VelocityX = std::min(0.0f, m_VelocityX + activeFriction * dt);
    }

    if (m_OnGround && hasDirectionalInput && (m_VelocityX * inputDirection) < 0.0f &&
        std::abs(m_VelocityX) < 8.0f) {
        m_VelocityX = 0.0f;
    }

    m_VelocityX = std::clamp(m_VelocityX, -activeRunSpeed, activeRunSpeed);

    const bool jumpPressed =
        !(m_GoalWalkActive && !m_GoalWalkReached) &&
        Util::Input::IsKeyDown(Util::Keycode::SPACE);
    const bool jumpHeld =
        !(m_GoalWalkActive && !m_GoalWalkReached) &&
        Util::Input::IsKeyPressed(Util::Keycode::SPACE);

    if (m_TransformType == TransformType::None &&
        !m_IsCrouching && jumpPressed && m_OnGround) {
        m_VelocityY = m_JumpForce;
        m_OnGround = false;
        m_JumpTimer = m_MaxJumpTime;
    }

    else if (m_TransformType == TransformType::None &&
             jumpHeld && m_JumpTimer > 0.0f && m_VelocityY > 0.0f) {
        m_VelocityY += SMB3_JUMP_HOLD_FORCE * dt;
        m_JumpTimer = std::max(0.0f, m_JumpTimer - dt);
    } else {
        m_JumpTimer = 0.0f;
    }

    m_VelocityY += m_Gravity * dt;

    float moveX = m_VelocityX * dt;
    float moveY = m_VelocityY * dt;

    // axis-separated collision resolution:
    // - first resolve horizontal movement (with current Y)
    // - then resolve vertical movement (with resolved X)

    const bool wasOnGround = m_OnGround;
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
    const auto movingPlatforms = g_MapManager ? g_MapManager->GetMovingPlatformSnapshots()
                                              : std::vector<MapManager::MovingPlatformSnapshot>{};
    if (g_MapManager && wasOnGround && m_VelocityY <= 0.0f) {
        const glm::vec2 carryDelta = g_MapManager->GetCarryDelta({ curX, curY }, halfExtents);
        curX += carryDelta.x;
        curY += carryDelta.y;
        candidateX = curX;
        candidateY = curY;
    }

    auto resolveHorizontalPlatforms = [&](float currentY, float& targetX) {
        if (movingPlatforms.empty() || std::abs(targetX - curX) <= 0.0f) return;

        const float actorTop = currentY + halfHeight;
        const float actorBottom = currentY - halfHeight;
        for (const auto& platform : movingPlatforms) {
            if (platform.wrappedThisFrame) continue;
            const float platformLeft = platform.center.x - platform.halfExtents.x;
            const float platformRight = platform.center.x + platform.halfExtents.x;
            const float platformTop = platform.center.y + platform.halfExtents.y;
            const float platformBottom = platform.center.y - platform.halfExtents.y;
            const bool overlapY = actorTop > platformBottom + 2.0f && actorBottom < platformTop - 2.0f;
            if (!overlapY) continue;

            if (targetX > curX) {
                const float previousRight = curX + halfWidth;
                const float nextRight = targetX + halfWidth;
                if (previousRight <= platformLeft + 3.0f && nextRight > platformLeft) {
                    targetX = platformLeft - halfWidth - eps;
                    m_VelocityX = 0.0f;
                }
            } else {
                const float previousLeft = curX - halfWidth;
                const float nextLeft = targetX - halfWidth;
                if (previousLeft >= platformRight - 3.0f && nextLeft < platformRight) {
                    targetX = platformRight + halfWidth + eps;
                    m_VelocityX = 0.0f;
                }
            }
        }
    };

    auto resolveVerticalPlatforms = [&](float& targetY) {
        if (movingPlatforms.empty()) return;

        const float previousBottom = curY - halfHeight;
        const float previousTop = curY + halfHeight;
        const float actorLeft = candidateX - halfWidth;
        const float actorRight = candidateX + halfWidth;

        for (const auto& platform : movingPlatforms) {
            if (platform.wrappedThisFrame) continue;
            const float platformLeft = platform.center.x - platform.halfExtents.x;
            const float platformRight = platform.center.x + platform.halfExtents.x;
            const float platformTop = platform.center.y + platform.halfExtents.y;
            const float platformBottom = platform.center.y - platform.halfExtents.y;
            const float previousPlatformTop = platformTop - platform.delta.y;
            const float previousPlatformBottom = platformBottom - platform.delta.y;
            const bool overlapX = actorRight > platformLeft + 2.0f && actorLeft < platformRight - 2.0f;
            if (!overlapX) continue;

            if (moveY <= 0.0f || platform.delta.y > 0.0f) {
                const float nextBottom = targetY - halfHeight;
                if (previousBottom >= previousPlatformTop - 6.0f &&
                    nextBottom <= platformTop + 4.0f) {
                    targetY = platformTop + halfHeight;
                    m_VelocityY = 0.0f;
                    m_OnGround = true;
                    return;
                }
            }

            if (moveY > 0.0f) {
                const float nextTop = targetY + halfHeight;
                if (previousTop <= previousPlatformBottom + 6.0f &&
                    nextTop >= platformBottom - 2.0f) {
                    const float ceilingSeparation = IsBig() ? 0.5f : 1.0f;
                    targetY = platformBottom - halfHeight - ceilingSeparation;
                    m_VelocityY = 0.0f;
                    m_JumpTimer = 0.0f;
                    return;
                }
            }
        }
    };

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
    resolveHorizontalPlatforms(curY, candidateX);

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
                    const float ceilingSeparation = IsBig() ? 0.5f : 1.0f;
                    candidateY = tileBottom - halfHeight - ceilingSeparation;
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
    resolveVerticalPlatforms(candidateY);

    // Apply resolved candidate position
    m_Transform.translation.x = candidateX;
    m_Transform.translation.y = candidateY;

    if (m_GoalWalkActive && m_Transform.translation.x >= m_GoalWalkTargetX - 0.5f) {
        m_Transform.translation.x = m_GoalWalkTargetX;
        m_VelocityX = 0.0f;
        m_GoalWalkActive = false;
        m_GoalWalkReached = true;
    }

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
            Die(false);
            return;
        }
    }

    HandleAnimation(dt);
}

