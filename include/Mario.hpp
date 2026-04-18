#ifndef MARIO_HPP
#define MARIO_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Animation.hpp"
#include "MapManager.hpp"
#include <memory>
#include <map>

class Mario : public Util::GameObject {
public:
    enum class AnimState { IDLE, WALK, JUMP, BRAKE, CROUCH };
    enum class PowerState { Small, Big, Fire };
    enum class TransformType { None, SmallBigTransition, FireTransition, StarTransition };
    enum class GoalSequenceState { None, SlideDownFlag, CrossPole, DropFromPole, Finished };

    Mario();
    void Update();
    void SetSpawnPosition(const glm::vec2& spawn) { m_SpawnPosition = spawn; }
    void PowerUp(LootType type);
    void TakeEnemyHit();
    void Die();
    void BounceAfterStomp();
    void StartGoalSequence(float poleX, float flagX, float flagBottomY, float groundY, float slideStartY);
    bool IsGoalSequenceActive() const {
        return m_GoalSequenceState != GoalSequenceState::None &&
               m_GoalSequenceState != GoalSequenceState::Finished;
    }
    bool IsGoalSequenceFinished() const { return m_GoalSequenceState == GoalSequenceState::Finished; }
    float GetFacingDirection() const { return m_Transform.scale.x >= 0.0f ? 1.0f : -1.0f; }
    float GetRenderOffsetY() const;
    glm::vec2 GetFireballSpawnPosition() const;
    bool IsDead() const { return m_IsDead; }
    bool IsBig() const { return m_PowerState != PowerState::Small; }
    bool IsFire() const { return m_PowerState == PowerState::Fire; }
    bool IsTransforming() const { return m_TransformType != TransformType::None; }
    bool IsInvulnerable() const { return m_InvulnerabilityTimer > 0.0f || m_StarPowerTimer > 0.0f; }
    bool HasStarPower() const { return m_StarPowerTimer > 0.0f; }
    glm::vec2 GetHalfExtents() const;
    float GetVelocityY() const { return m_VelocityY; }
    bool WasJumpingUpward() const { return m_VelocityY > 0.0f; }

private:
    static constexpr float SMALL_HALF_WIDTH = 18.0f;
    static constexpr float SMALL_HALF_HEIGHT = 24.0f;
    static constexpr float BIG_HALF_WIDTH = 18.0f;
    static constexpr float BIG_HALF_HEIGHT = 40.0f;
    static constexpr float CROUCH_HALF_HEIGHT = BIG_HALF_HEIGHT * (22.0f / 32.0f);
    static constexpr float BIG_RENDER_OFFSET_Y = 5.0f;
    static constexpr float INVULNERABILITY_DURATION = 3.0f;

    float m_VelocityX, m_VelocityY;
    float m_Acceleration, m_MaxSpeed, m_Friction, m_Gravity, m_JumpForce;
    bool m_OnGround;
    bool m_IsBraking = false;
    bool m_IsCrouching = false;
    bool m_IsDead = false;
    float m_JumpTimer, m_MaxJumpTime;
    float m_StoredVelocityX = 0.0f;
    float m_StoredVelocityY = 0.0f;
    float m_RespawnTimer = 0.0f;
    float m_InvulnerabilityTimer = 0.0f;
    float m_StarPowerTimer = 0.0f;
    float m_PowerDownLockTimer = 0.0f;
    float m_TransformTimer = 0.0f;
    glm::vec2 m_SpawnPosition = { 0.0f, -250.0f };
    PowerState m_PowerState = PowerState::Small;
    PowerState m_TargetPowerState = PowerState::Small;
    TransformType m_TransformType = TransformType::None;
    AnimState m_TransformAnimState = AnimState::IDLE;
    bool m_TransformShowBigFrame = false;
    GoalSequenceState m_GoalSequenceState = GoalSequenceState::None;
    float m_GoalPoleX = 0.0f;
    float m_GoalFlagX = 0.0f;
    float m_GoalFlagBottomY = 0.0f;
    float m_GoalGroundY = 0.0f;
    float m_GoalSlideStartY = 0.0f;

    AnimState m_AnimState = AnimState::IDLE;
    std::map<AnimState, std::unique_ptr<Animation>> m_SmallAnimations;
    std::map<AnimState, std::unique_ptr<Animation>> m_BigAnimations;
    std::map<AnimState, std::unique_ptr<Animation>> m_FireAnimations;
    std::map<PowerState, std::unique_ptr<Animation>> m_FlagAnimations;

    std::shared_ptr<Util::Image> m_Image;
    std::unique_ptr<Animation> m_TransformAnimation;
    std::string m_SizeTransitionFramePath;
    std::string m_DeathFramePath;

    std::map<AnimState, std::unique_ptr<Animation>>& ActiveAnimations();
    const std::map<AnimState, std::unique_ptr<Animation>>& ActiveAnimations() const;
    std::map<AnimState, std::unique_ptr<Animation>>& AnimationsForPowerState(PowerState state);
    const std::map<AnimState, std::unique_ptr<Animation>>& AnimationsForPowerState(PowerState state) const;
    void ResetAnimations();
    void BeginTransformation(PowerState targetState, TransformType transformType);
    void SetPowerState(PowerState newState);
    void ActivateStarPower();
    void RestoreStoredMotion();
    void HandleAnimation(float dt);
    Animation& ActiveFlagAnimation();
    const Animation& ActiveFlagAnimation() const;
    std::string CurrentAnimatedFramePath() const;
    std::string FramePathForState(PowerState powerState, AnimState animState) const;
    std::string DisplayFramePathForBase(const std::string& basePath) const;
};

#endif
