#include "Enemy.hpp"
#include "EnemyDetail.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

using namespace EnemyDetail;

void Enemy::Stomp() {
    if (!m_Alive) return;

    if (IsKoopa()) {
        if (m_State == State::Walking) {
            EnterRetreatingIntoShell();
        } else if (m_State == State::ShellSliding) {
            EnterShellIdle();
        } else if (m_State == State::ShellIdle || m_State == State::Recovering) {
            m_ShellIdleTimer = KOOPA_SHELL_IDLE_DURATION;
            m_RecoveryTimer = 0.0f;
            RefreshSprite();
        }
        return;
    }

    m_Alive = false;
    m_State = State::Dead;
    m_FlippedDeath = false;
    m_DeathFinished = false;
    m_VelocityX = 0.0f;
    m_DeathTimer = 0.5f;
    m_Image->SetImage(m_DeathPath);
}

void Enemy::KillFlipped(float horizontalVelocity) {
    m_Alive = false;
    m_State = State::Dead;
    m_FlippedDeath = true;
    m_DeathFinished = false;
    m_DeathTimer = 0.0f;
    m_VelocityX = horizontalVelocity;
    m_VelocityY = FLIPPED_DEATH_LAUNCH_Y;
    m_Transform.scale.x = -std::abs(m_Transform.scale.x);
    m_Transform.scale.y = -std::abs(m_Transform.scale.y);
    m_Image->SetImage(IsKoopa() ? m_LeftPath : (m_UseLeftWalkFrame ? m_LeftPath : m_RightPath));
}

void Enemy::RefreshSprite() {
    if (!m_Alive) {
        return;
    }

    constexpr float walkFrameDuration = 0.12f;
    m_WalkAnimationTimer += Util::Time::GetDeltaTimeMs() / 1000.0f;
    if (m_WalkAnimationTimer >= walkFrameDuration) {
        m_WalkAnimationTimer = std::fmod(m_WalkAnimationTimer, walkFrameDuration);
        m_UseLeftWalkFrame = !m_UseLeftWalkFrame;
    }

    if (m_Kind == EnemyKind::Goomba) {
        const std::string& path = m_UseLeftWalkFrame ? m_LeftPath : m_RightPath;
        m_Image->SetImage(path);
        return;
    }

    m_Transform.scale.x =
        (m_Direction >= 0.0f) ? -std::abs(m_Transform.scale.x) : std::abs(m_Transform.scale.x);

    if (m_State == State::RetreatingIntoShell ||
        m_State == State::ShellIdle ||
        m_State == State::ShellSliding) {
        m_Image->SetImage(m_ShellPath);
        return;
    }

    if (m_State == State::Recovering) {
        const bool showRecoverSprite = std::fmod(m_RecoveryTimer, 0.16f) < 0.08f;
        if (showRecoverSprite) {
            m_Image->SetImage(m_ShellRecoverPath);
        } else {
            m_Image->SetImage(m_ShellPath);
        }
        return;
    }

    m_Image->SetImage(m_UseLeftWalkFrame ? m_LeftPath : m_RightPath);
}
