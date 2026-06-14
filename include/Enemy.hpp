#pragma once

#include "GameImage.hpp"
#include "Util/GameObject.hpp"
#include <string>
#include <memory>

enum class EnemyKind {
    Goomba,
    GreenKoopa,
    RedKoopa,
    RedKoopaWinged,
    Venus
};

struct EnemySpawnInfo {
    glm::vec2 position;
    EnemyKind kind = EnemyKind::Goomba;
    float flightTopTiles = 2.0f;
    float flightBottomTiles = 2.0f;
};

class Enemy : public Util::GameObject {
public:
    enum class State {
        Walking,
        RetreatingIntoShell,
        ShellIdle,
        ShellSliding,
        Recovering,
        Dead
    };

    Enemy(float x,
          float y,
          EnemyKind kind = EnemyKind::Goomba,
          float flightTopTiles = 2.0f,
          float flightBottomTiles = 2.0f);
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
    bool IsKoopa() const {
        return m_Kind == EnemyKind::GreenKoopa ||
               m_Kind == EnemyKind::RedKoopa ||
               m_Kind == EnemyKind::RedKoopaWinged;
    }
    bool IsWingedKoopa() const { return m_Kind == EnemyKind::RedKoopaWinged; }
    bool IsVenus() const { return m_Kind == EnemyKind::Venus; }
    bool UsesEdgeDetection() const { return m_Kind == EnemyKind::RedKoopa; }
    bool IsShellSliding() const { return IsKoopa() && m_State == State::ShellSliding; }
    bool IsKickableShell() const {
        return IsKoopa() &&
               (m_State == State::RetreatingIntoShell ||
                m_State == State::ShellIdle ||
                m_State == State::Recovering);
    }
    bool IsHarmlessToPlayer() const {
        if (IsVenus()) {
            return m_VenusExposure < 0.35f;
        }
        if (m_HarmlessTimer > 0.0f) {
            return true;
        }
        return IsKoopa() &&
               (m_State == State::RetreatingIntoShell ||
                m_State == State::ShellIdle ||
                m_State == State::Recovering);
    }
    bool CanBeDefeatedByShell() const { return m_Alive; }

private:
    std::shared_ptr<GameImage> m_Image;
    EnemyKind m_Kind = EnemyKind::Goomba;
    State m_State = State::Walking;
    float m_Direction = -1.0f;
    float m_Speed = 40.0f;
    float m_VelocityY = 0.0f;
    float m_VelocityX = 0.0f;
    float m_DeathTimer = 0.0f;
    float m_WalkAnimationTimer = 0.0f;
    float m_RetreatTimer = 0.0f;
    float m_ShellIdleTimer = 0.0f;
    float m_RecoveryTimer = 0.0f;
    float m_VenusTimer = 0.0f;
    float m_VenusHiddenY = 0.0f;
    float m_VenusTopY = 0.0f;
    float m_VenusExposure = 1.0f;
    float m_FlightOriginY = 0.0f;
    float m_FlightTopY = 0.0f;
    float m_FlightBottomY = 0.0f;
    float m_FlightDirection = -1.0f;
    float m_HarmlessTimer = 0.0f;
    bool m_Alive = true;
    bool m_FlippedDeath = false;
    bool m_DeathFinished = false;
    bool m_UseLeftWalkFrame = true;
    std::string m_LeftPath;
    std::string m_RightPath;
    std::string m_ShellPath;
    std::string m_ShellRecoverPath;
    std::string m_DeathPath;

    void EnterRetreatingIntoShell();
    void EnterShellIdle();
    void EnterRecovering();
    void LoseWings();
    void RefreshSprite();
};

