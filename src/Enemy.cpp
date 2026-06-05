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
      m_Speed((kind == EnemyKind::GreenKoopa || kind == EnemyKind::RedKoopa) ? KOOPA_WALK_SPEED : GOOMBA_WALK_SPEED),
      m_LeftPath((g_MapManager && g_MapManager->IsUndergroundTheme())
          ? (kind == EnemyKind::Venus ? AssetPaths::Image("ug_venus.png")
             : (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa1.png")
                : ((kind == EnemyKind::GreenKoopa) ? AssetPaths::Image("ug_koopa_l.png") : AssetPaths::Image("ug_goomba_l.png"))))
          : (kind == EnemyKind::Venus ? AssetPaths::Image("ug_venus.png")
             : (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa1.png")
                : ((kind == EnemyKind::GreenKoopa) ? AssetPaths::Image("koopa_l.png") : AssetPaths::Image("Goomba_l.png"))))),
      m_RightPath((g_MapManager && g_MapManager->IsUndergroundTheme())
          ? (kind == EnemyKind::Venus ? AssetPaths::Image("ug_venus2.png")
             : (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa2.png")
                : ((kind == EnemyKind::GreenKoopa) ? AssetPaths::Image("ug_koopa_r.png") : AssetPaths::Image("ug_goomba_r.png"))))
          : (kind == EnemyKind::Venus ? AssetPaths::Image("ug_venus2.png")
             : (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa2.png")
                : ((kind == EnemyKind::GreenKoopa) ? AssetPaths::Image("koopa_r.png") : AssetPaths::Image("Goomba_r.png"))))),
      m_ShellPath((g_MapManager && g_MapManager->IsUndergroundTheme())
          ? (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa_shell.png")
             : ((kind == EnemyKind::GreenKoopa) ? AssetPaths::Image("ug_koopa_shell.png") : AssetPaths::Image("ug_goomba_death.png")))
          : (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa_shell.png")
             : ((kind == EnemyKind::GreenKoopa) ? AssetPaths::Image("koopa_shell.png") : AssetPaths::Image("Goombadeath.png")))),
      m_ShellRecoverPath((g_MapManager && g_MapManager->IsUndergroundTheme())
          ? (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa_shell2.png") : AssetPaths::Image("ug_koopa_shell2.png"))
          : (kind == EnemyKind::RedKoopa ? AssetPaths::Image("redkoopa_shell2.png") : AssetPaths::Image("koopa_shell2.png"))),
      m_DeathPath((g_MapManager && g_MapManager->IsUndergroundTheme())
          ? AssetPaths::Image("ug_goomba_death.png")
          : AssetPaths::Image("Goombadeath.png")) {
    m_Image = std::make_shared<Util::Image>(m_LeftPath);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 9.0f;
    if (m_Kind == EnemyKind::Venus) {
        m_VenusTopY = y;
        m_VenusHiddenY = y - 96.0f;
        m_VenusExposure = 0.0f;
        m_Transform.translation.y = m_VenusHiddenY;
        m_ZIndex = 0.5f;
    }
    RefreshSprite();
}

glm::vec2 Enemy::GetHalfExtents() const {
    if (m_Kind == EnemyKind::Venus) {
        return { 22.0f, 28.0f };
    }
    if (m_Kind == EnemyKind::Goomba) {
        return { 20.0f, 24.0f };
    }
    if (m_State == State::RetreatingIntoShell ||
        m_State == State::ShellIdle ||
        m_State == State::ShellSliding ||
        m_State == State::Recovering) {
        return { 18.0f, 18.0f };
    }
    return { 24.0f, 36.0f };
}

void Enemy::SetDirection(float direction) {
    if (direction == 0.0f) return;
    m_Direction = (direction > 0.0f) ? 1.0f : -1.0f;
    RefreshSprite();
}

void Enemy::EnterRetreatingIntoShell() {
    if (!IsKoopa()) return;
    m_State = State::RetreatingIntoShell;
    m_RetreatTimer = KOOPA_RETREAT_DURATION;
    m_ShellIdleTimer = 0.0f;
    m_RecoveryTimer = 0.0f;
    m_VelocityX = 0.0f;
    RefreshSprite();
}

void Enemy::EnterShellIdle() {
    if (!IsKoopa()) return;
    m_State = State::ShellIdle;
    m_RetreatTimer = 0.0f;
    m_ShellIdleTimer = KOOPA_SHELL_IDLE_DURATION;
    m_RecoveryTimer = 0.0f;
    m_VelocityX = 0.0f;
    RefreshSprite();
}

void Enemy::EnterRecovering() {
    if (!IsKoopa()) return;
    m_State = State::Recovering;
    m_RetreatTimer = 0.0f;
    m_ShellIdleTimer = 0.0f;
    m_RecoveryTimer = KOOPA_RECOVERY_DURATION;
    m_VelocityX = 0.0f;
    RefreshSprite();
}

void Enemy::KickShell(float direction) {
    if (!IsKoopa()) return;
    if (m_State != State::RetreatingIntoShell &&
        m_State != State::ShellIdle &&
        m_State != State::Recovering) return;

    m_State = State::ShellSliding;
    m_RetreatTimer = 0.0f;
    m_Direction = (direction >= 0.0f) ? 1.0f : -1.0f;
    m_VelocityX = m_Direction * KOOPA_SHELL_SPEED;
    m_ShellIdleTimer = KOOPA_SHELL_IDLE_DURATION;
    m_RecoveryTimer = 0.0f;
    RefreshSprite();
}
