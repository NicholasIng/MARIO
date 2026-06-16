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

void Mario::Die(bool launchUpward) {
    if (m_DebugGodMode) return;
    if (m_IsDead) return;
    m_IsDead = true;
    m_DeathFinished = false;
    m_RespawnTimer = launchUpward ? 1.25f : 0.75f;
    m_VelocityX = 0.0f;
    m_VelocityY = launchUpward ? 900.0f : 0.0f;
    m_IsCrouching = false;
    m_OnGround = false;
    m_JumpTimer = 0.0f;
    m_InvulnerabilityTimer = 0.0f;
    m_StarPowerTimer = 0.0f;
    m_PowerDownLockTimer = 0.0f;
    m_TransformType = TransformType::None;
    m_GoalSequenceState = GoalSequenceState::None;
    m_GoalWalkActive = false;
    m_GoalWalkReached = false;
    SetVisible(launchUpward);
    if (launchUpward) {
        m_Image->SetImage(m_DeathFramePath);
    }
}

void Mario::BounceAfterStomp() {
    m_VelocityY = m_JumpForce * 0.6f;
    m_OnGround = false;
    m_JumpTimer = 0.0f;
}

void Mario::StartGoalWalk(float targetX) {
    m_GoalWalkTargetX = targetX;
    m_GoalWalkActive = true;
    m_GoalWalkReached = false;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
    m_JumpTimer = 0.0f;
    m_IsCrouching = false;
    m_Transform.scale.x = std::abs(m_Transform.scale.x);
    ActiveAnimations().at(AnimState::WALK)->Reset();
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
    m_GoalWalkActive = false;
    m_GoalWalkReached = false;
    m_GoalSequenceState = GoalSequenceState::SlideDownFlag;
    m_Transform.scale.x = std::abs(m_Transform.scale.x);
    m_Transform.translation.x = m_GoalFlagX + FLAG_SLIDE_OFFSET_X;
    const float standingY = m_GoalGroundY + GetHalfExtents().y;
    m_Transform.translation.y = std::max(m_GoalSlideStartY, standingY);
    ActiveFlagAnimation().Reset();
    SetVisible(true);
}

