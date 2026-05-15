#include "Enemy.hpp"
#include "EnemyDetail.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

using namespace EnemyDetail;

Enemy::Enemy(float x, float y, EnemyKind kind)
    : m_Kind(kind),
      m_State(State::Walking),
      m_Direction(-1.0f),
      m_Speed(kind == EnemyKind::Koopa ? KOOPA_WALK_SPEED : GOOMBA_WALK_SPEED),
      m_LeftPath(kind == EnemyKind::Koopa ? AssetPaths::Image("koopa_l.png") : AssetPaths::Image("Goomba_l.png")),
      m_RightPath(kind == EnemyKind::Koopa ? AssetPaths::Image("koopa_r.png") : AssetPaths::Image("Goomba_r.png")),
      m_ShellPath(kind == EnemyKind::Koopa ? AssetPaths::Image("koopa_shell.png") : AssetPaths::Image("Goombadeath.png")),
      m_DeathPath(AssetPaths::Image("Goombadeath.png")) {
    m_Image = std::make_shared<Util::Image>(m_LeftPath);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 9.0f;
    RefreshSprite();
}

glm::vec2 Enemy::GetHalfExtents() const {
    if (m_Kind == EnemyKind::Goomba) {
        return { 20.0f, 24.0f };
    }
    if (m_State == State::ShellIdle || m_State == State::ShellSliding || m_State == State::Recovering) {
        return { 18.0f, 18.0f };
    }
    return { 24.0f, 36.0f };
}

void Enemy::SetDirection(float direction) {
    if (direction == 0.0f) return;
    m_Direction = (direction > 0.0f) ? 1.0f : -1.0f;
    RefreshSprite();
}

void Enemy::EnterShellIdle() {
    if (m_Kind != EnemyKind::Koopa) return;
    m_State = State::ShellIdle;
    m_ShellIdleTimer = KOOPA_SHELL_IDLE_DURATION;
    m_RecoveryTimer = 0.0f;
    m_VelocityX = 0.0f;
    RefreshSprite();
}

void Enemy::EnterRecovering() {
    if (m_Kind != EnemyKind::Koopa) return;
    m_State = State::Recovering;
    m_ShellIdleTimer = 0.0f;
    m_RecoveryTimer = KOOPA_RECOVERY_DURATION;
    m_VelocityX = 0.0f;
    RefreshSprite();
}

void Enemy::KickShell(float direction) {
    if (m_Kind != EnemyKind::Koopa) return;
    if (m_State != State::ShellIdle && m_State != State::Recovering) return;

    m_State = State::ShellSliding;
    m_Direction = (direction >= 0.0f) ? 1.0f : -1.0f;
    m_VelocityX = m_Direction * KOOPA_SHELL_SPEED;
    m_ShellIdleTimer = 0.0f;
    m_RecoveryTimer = 0.0f;
    RefreshSprite();
}
