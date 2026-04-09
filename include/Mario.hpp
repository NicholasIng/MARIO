#ifndef MARIO_HPP
#define MARIO_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Animation.hpp"
#include <memory>
#include <map>

class Mario : public Util::GameObject {
public:
    enum class AnimState { IDLE, WALK, JUMP, BRAKE, CROUCH };

    Mario();
    void Update();
    void SetSpawnPosition(const glm::vec2& spawn) { m_SpawnPosition = spawn; }
    void Die();
    void BounceAfterStomp();
    bool IsDead() const { return m_IsDead; }
    glm::vec2 GetHalfExtents() const { return { 20.0f, 24.0f }; }
    float GetVelocityY() const { return m_VelocityY; }
    bool WasJumpingUpward() const { return m_VelocityY > 0.0f; }

private:
    float m_VelocityX, m_VelocityY;
    float m_Acceleration, m_MaxSpeed, m_Friction, m_Gravity, m_JumpForce;
    bool m_OnGround;
    bool m_IsCrouching = false;
    bool m_IsDead = false;
    float m_JumpTimer, m_MaxJumpTime;
    float m_RespawnTimer = 0.0f;
    glm::vec2 m_SpawnPosition = { 0.0f, -250.0f };

    AnimState m_AnimState = AnimState::IDLE;
    std::map<AnimState, std::unique_ptr<Animation>> m_Animations;

    std::shared_ptr<Util::Image> m_Image;

    void HandleAnimation(float dt);
};

#endif
