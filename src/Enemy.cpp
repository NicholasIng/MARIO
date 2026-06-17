#include "Enemy.hpp"
#include "EnemyDetail.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

using namespace EnemyDetail;

namespace {
bool IsUndergroundEnemyTheme() {
    return g_MapManager && g_MapManager->IsUndergroundTheme();
}

std::string ResolveEnemyLeftPath(EnemyKind kind) {
    if (kind == EnemyKind::Venus) {
        return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_venus.png" : "venus_green.png");
    }
    if (kind == EnemyKind::RedKoopaWinged) return AssetPaths::Image("redkoopa_wing1.png");
    if (kind == EnemyKind::RedKoopa) return AssetPaths::Image("redkoopa1.png");
    if (kind == EnemyKind::GreenKoopa) {
        return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_koopa_l.png" : "koopa_l.png");
    }
    return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_goomba_l.png" : "Goomba_l.png");
}

std::string ResolveEnemyRightPath(EnemyKind kind) {
    if (kind == EnemyKind::Venus) {
        return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_venus2.png" : "venus_green2.png");
    }
    if (kind == EnemyKind::RedKoopaWinged) return AssetPaths::Image("redkoopa_wing2.png");
    if (kind == EnemyKind::RedKoopa) return AssetPaths::Image("redkoopa2.png");
    if (kind == EnemyKind::GreenKoopa) {
        return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_koopa_r.png" : "koopa_r.png");
    }
    return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_goomba_r.png" : "Goomba_r.png");
}

std::string ResolveEnemyShellPath(EnemyKind kind) {
    if (kind == EnemyKind::RedKoopa || kind == EnemyKind::RedKoopaWinged) {
        return AssetPaths::Image("redkoopa_shell.png");
    }
    if (kind == EnemyKind::GreenKoopa) {
        return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_koopa_shell.png" : "koopa_shell.png");
    }
    return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_goomba_death.png" : "Goombadeath.png");
}

std::string ResolveEnemyShellRecoverPath(EnemyKind kind) {
    if (kind == EnemyKind::RedKoopa || kind == EnemyKind::RedKoopaWinged) {
        return AssetPaths::Image("redkoopa_shell2.png");
    }
    return AssetPaths::Image(IsUndergroundEnemyTheme() ? "ug_koopa_shell2.png" : "koopa_shell2.png");
}
}

Enemy::Enemy(float x, float y, EnemyKind kind, float flightTopTiles, float flightBottomTiles)
    : m_Kind(kind),
      m_State(State::Walking),
      m_Direction(-1.0f),
      m_Speed((kind == EnemyKind::GreenKoopa || kind == EnemyKind::RedKoopa || kind == EnemyKind::RedKoopaWinged)
          ? KOOPA_WALK_SPEED
          : GOOMBA_WALK_SPEED),
      m_LeftPath(ResolveEnemyLeftPath(kind)),
      m_RightPath(ResolveEnemyRightPath(kind)),
      m_ShellPath(ResolveEnemyShellPath(kind)),
      m_ShellRecoverPath(ResolveEnemyShellRecoverPath(kind)),
      m_DeathPath((g_MapManager && g_MapManager->IsUndergroundTheme())
          ? AssetPaths::Image("ug_goomba_death.png")
          : AssetPaths::Image("Goombadeath.png")) {
    m_Image = std::make_shared<GameImage>(m_LeftPath);
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
    if (m_Kind == EnemyKind::RedKoopaWinged) {
        const float tileSize = g_MapManager ? g_MapManager->GetTileSize() : 48.0f;
        m_FlightOriginY = y;
        m_FlightTopY = y + std::max(0.0f, flightTopTiles) * tileSize;
        m_FlightBottomY = y - std::max(0.0f, flightBottomTiles) * tileSize;
        m_FlightDirection = -1.0f;
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

void Enemy::LoseWings() {
    if (!IsWingedKoopa()) return;

    m_Kind = EnemyKind::RedKoopa;
    m_State = State::Walking;
    m_LeftPath = AssetPaths::Image("redkoopa1.png");
    m_RightPath = AssetPaths::Image("redkoopa2.png");
    m_ShellPath = AssetPaths::Image("redkoopa_shell.png");
    m_ShellRecoverPath = AssetPaths::Image("redkoopa_shell2.png");
    m_Speed = KOOPA_WALK_SPEED;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
    m_RetreatTimer = 0.0f;
    m_ShellIdleTimer = 0.0f;
    m_RecoveryTimer = 0.0f;
    m_HarmlessTimer = 0.35f;
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
