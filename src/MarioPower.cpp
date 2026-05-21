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

void Mario::RestorePowerState(PowerState state) {
    m_TargetPowerState = state;
    m_TransformType = TransformType::None;
    m_TransformTimer = 0.0f;
    m_TransformShowBigFrame = false;
    m_StoredVelocityX = 0.0f;
    m_StoredVelocityY = 0.0f;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
    m_InvulnerabilityTimer = 0.0f;
    m_StarPowerTimer = 0.0f;
    m_PowerDownLockTimer = 0.0f;
    m_IsCrouching = false;
    m_OnGround = false;
    SetVisible(true);

    if (m_PowerState != state) {
        SetPowerState(state);
    } else {
        ResetAnimations();
        m_AnimState = AnimState::IDLE;
        m_Image->SetImage(ActiveAnimations().at(m_AnimState)->GetCurrentFramePath());
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

