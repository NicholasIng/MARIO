#pragma once

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <string>
#include <memory>

enum class EnemyKind {
    Goomba,
    Koopa
};

struct EnemySpawnInfo {
    glm::vec2 position;
    EnemyKind kind = EnemyKind::Goomba;
};

class Enemy : public Util::GameObject {
public:
    enum class State {
        Walking,
        ShellIdle,
        ShellSliding,
        Recovering
    };

    Enemy(float x, float y, EnemyKind kind = EnemyKind::Goomba);
    void Update();
    void Stomp();
    void KickShell(float direction);
    void KillFlipped(float horizontalVelocity = 0.0f);
    bool IsAlive() const { return m_Alive; }
    bool IsDeadAndExpired() const { return !m_Alive && m_DeathFinished; }
    glm::vec2 GetHalfExtents() const;
    float GetDirection() const { return m_Direction; }
    void SetDirection(float direction);
    float GetHorizontalVelocity() const { return m_VelocityX; }
    EnemyKind GetKind() const { return m_Kind; }
    State GetState() const { return m_State; }
    bool IsShellSliding() const { return m_Kind == EnemyKind::Koopa && m_State == State::ShellSliding; }
    bool IsKickableShell() const {
        return m_Kind == EnemyKind::Koopa &&
               (m_State == State::ShellIdle || m_State == State::Recovering);
    }
    bool IsHarmlessToPlayer() const {
        return m_Kind == EnemyKind::Koopa &&
               (m_State == State::ShellIdle ||
                m_State == State::ShellSliding ||
                m_State == State::Recovering);
    }
    bool CanBeDefeatedByShell() const { return m_Alive; }

private:
    std::shared_ptr<Util::Image> m_Image;
    EnemyKind m_Kind = EnemyKind::Goomba;
    State m_State = State::Walking;
    float m_Direction = -1.0f;
    float m_Speed = 40.0f;
    float m_VelocityY = 0.0f;
    float m_VelocityX = 0.0f;
    float m_DeathTimer = 0.0f;
    float m_WalkAnimationTimer = 0.0f;
    float m_ShellIdleTimer = 0.0f;
    float m_RecoveryTimer = 0.0f;
    bool m_Alive = true;
    bool m_FlippedDeath = false;
    bool m_DeathFinished = false;
    bool m_UseLeftWalkFrame = true;
    std::string m_LeftPath;
    std::string m_RightPath;
    std::string m_ShellPath;
    std::string m_DeathPath;

    void EnterShellIdle();
    void EnterRecovering();
    void RefreshSprite();
};

