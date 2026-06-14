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
    else if (m_IsBraking) m_AnimState = AnimState::BRAKE;
    else if (std::abs(m_VelocityX) > 20.0f) {
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
