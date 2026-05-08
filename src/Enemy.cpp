#include "Enemy.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {
constexpr float FLIPPED_DEATH_GRAVITY = -1800.0f;
constexpr float FLIPPED_DEATH_LAUNCH_Y = 520.0f;
constexpr float FLIPPED_DEATH_CULL_MARGIN = 96.0f;
constexpr float ENEMY_GRAVITY = -1800.0f;
constexpr float GOOMBA_WALK_SPEED = 40.0f;
constexpr float KOOPA_WALK_SPEED = 46.0f;
constexpr float KOOPA_SHELL_SPEED = 240.0f;
constexpr float KOOPA_SHELL_IDLE_DURATION = 4.0f;
constexpr float KOOPA_RECOVERY_DURATION = 1.1f;

bool IntersectsSolidTile(const glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return false;

    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
    const float eps = 0.001f;

    const int leftGridX = std::clamp(
        static_cast<int>(std::floor((center.x - halfExtents.x - mapLeft + eps) / tileSize)),
        0, std::max(0, g_MapManager->GetWidth() - 1)
    );
    const int rightGridX = std::clamp(
        static_cast<int>(std::floor((center.x + halfExtents.x - mapLeft - eps) / tileSize)),
        0, std::max(0, g_MapManager->GetWidth() - 1)
    );
    const int topGridY = std::clamp(
        static_cast<int>(std::floor((mapTop - (center.y + halfExtents.y - eps)) / tileSize)),
        0, std::max(0, g_MapManager->GetHeight() - 1)
    );
    const int bottomGridY = std::clamp(
        static_cast<int>(std::floor((mapTop - (center.y - halfExtents.y + eps)) / tileSize)),
        0, std::max(0, g_MapManager->GetHeight() - 1)
    );

    for (int gx = leftGridX; gx <= rightGridX; ++gx) {
        for (int gy = topGridY; gy <= bottomGridY; ++gy) {
            if (g_MapManager->IsSolidAt(gx, gy)) {
                return true;
            }
        }
    }

    return false;
}

void SnapUpOutOfGround(glm::vec2& center, const glm::vec2& halfExtents) {
    if (!g_MapManager) return;

    const int maxSteps = static_cast<int>(g_MapManager->GetTileSize() * 3.0f);
    for (int step = 0; step < maxSteps && IntersectsSolidTile(center, halfExtents); ++step) {
        center.y += 1.0f;
    }
}

} // namespace

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

void Enemy::Update() {
    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;

    if (!m_Alive) {
        if (m_FlippedDeath) {
            m_VelocityY += FLIPPED_DEATH_GRAVITY * dt;
            m_Transform.translation.x += m_VelocityX * dt;
            m_Transform.translation.y += m_VelocityY * dt;

            const float mapBottom = g_MapManager
                ? -(g_MapManager->GetHeight() * g_MapManager->GetTileSize()) / 2.0f
                : -1000.0f;
            if (m_Transform.translation.y + GetHalfExtents().y < mapBottom - FLIPPED_DEATH_CULL_MARGIN) {
                m_DeathFinished = true;
            }
        } else {
            m_DeathTimer -= dt;
            if (m_DeathTimer <= 0.0f) {
                m_DeathFinished = true;
            }
        }
        return;
    }

    if (m_Kind == EnemyKind::Koopa) {
        if (m_State == State::ShellIdle) {
            m_ShellIdleTimer = std::max(0.0f, m_ShellIdleTimer - dt);
            if (m_ShellIdleTimer <= 0.0f) {
                EnterRecovering();
            }
        } else if (m_State == State::Recovering) {
            m_RecoveryTimer = std::max(0.0f, m_RecoveryTimer - dt);
            if (m_RecoveryTimer <= 0.0f) {
                m_State = State::Walking;
                m_VelocityX = 0.0f;
                RefreshSprite();
            }
        }
    }

    SnapUpOutOfGround(m_Transform.translation, GetHalfExtents());
    if (!g_MapManager) {
        if (m_State == State::Walking) {
            m_Transform.translation.x += m_Direction * m_Speed * dt;
        } else if (m_State == State::ShellSliding) {
            m_Transform.translation.x += m_Direction * KOOPA_SHELL_SPEED * dt;
        }
        RefreshSprite();
        return;
    }

    const glm::vec2 half = GetHalfExtents();
    const float tileSize = g_MapManager->GetTileSize();
    const float mapLeft = g_MapManager->GetWorldLeft();
    const float mapTop = (g_MapManager->GetHeight() * tileSize) / 2.0f;
    const int mapWidth = g_MapManager->GetWidth();
    const int mapHeight = g_MapManager->GetHeight();
    const float eps = 0.001f;

    auto worldToGridX = [&](float worldX) {
        return static_cast<int>(std::floor((worldX - mapLeft) / tileSize));
    };
    auto worldToGridY = [&](float worldY) {
        return static_cast<int>(std::floor((mapTop - worldY) / tileSize));
    };
    auto hasGroundAhead = [&](float direction) {
        const float aheadX = m_Transform.translation.x + direction * (half.x + 4.0f);
        const float footY = m_Transform.translation.y - half.y - 2.0f;
        const int gridX = std::clamp(worldToGridX(aheadX), 0, std::max(0, mapWidth - 1));
        const int gridY = std::clamp(worldToGridY(footY), 0, std::max(0, mapHeight - 1));
        return g_MapManager->IsSolidAt(gridX, gridY);
    };

    m_VelocityY += ENEMY_GRAVITY * dt;

    float candidateY = m_Transform.translation.y + m_VelocityY * dt;
    float leftX = m_Transform.translation.x - half.x;
    float rightX = m_Transform.translation.x + half.x;
    int leftGridX = std::clamp(worldToGridX(leftX + eps), 0, std::max(0, mapWidth - 1));
    int rightGridX = std::clamp(worldToGridX(rightX - eps), 0, std::max(0, mapWidth - 1));
    bool onGround = false;

    if (m_VelocityY <= 0.0f) {
        float bottomEdge = candidateY - half.y;
        int gridY = std::clamp(worldToGridY(bottomEdge + eps), 0, std::max(0, mapHeight - 1));
        for (int gx = leftGridX; gx <= rightGridX; ++gx) {
            if (g_MapManager->IsSolidAt(gx, gridY)) {
                float tileTop = mapTop - gridY * tileSize;
                candidateY = tileTop + half.y;
                m_VelocityY = 0.0f;
                onGround = true;
                break;
            }
        }
    }
    m_Transform.translation.y = candidateY;

    const float mapBottom = -(g_MapManager->GetHeight() * tileSize) / 2.0f;
    if (m_Transform.translation.y + half.y < mapBottom - FLIPPED_DEATH_CULL_MARGIN) {
        m_Alive = false;
        m_DeathFinished = true;
        return;
    }

    if (m_State == State::Walking) {
        m_VelocityX = m_Direction * m_Speed;
    } else if (m_State == State::ShellSliding) {
        m_VelocityX = m_Direction * KOOPA_SHELL_SPEED;
    } else {
        m_VelocityX = 0.0f;
    }

    float candidateX = m_Transform.translation.x + m_VelocityX * dt;
    float topY = m_Transform.translation.y + half.y - eps;
    float bottomY = m_Transform.translation.y - half.y + eps;
    int topGridY = std::clamp(worldToGridY(topY - eps), 0, std::max(0, mapHeight - 1));
    int bottomGridY = std::clamp(worldToGridY(bottomY + eps), 0, std::max(0, mapHeight - 1));

    if (m_State == State::Walking || m_State == State::ShellSliding) {
        bool hitWall = false;
        if (m_Direction > 0.0f) {
            float rightEdge = candidateX + half.x;
            int gridX = std::clamp(worldToGridX(rightEdge - eps), 0, std::max(0, mapWidth - 1));
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                if (g_MapManager->IsSolidAt(gridX, gy)) {
                    hitWall = true;
                    break;
                }
            }
        } else {
            float leftEdgeAhead = candidateX - half.x;
            int gridX = std::clamp(worldToGridX(leftEdgeAhead + eps), 0, std::max(0, mapWidth - 1));
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                if (g_MapManager->IsSolidAt(gridX, gy)) {
                    hitWall = true;
                    break;
                }
            }
        }

        if (hitWall) {
            m_Direction = -m_Direction;
            if (m_State == State::ShellSliding) {
                m_VelocityX = m_Direction * KOOPA_SHELL_SPEED;
            } else {
                m_VelocityX = 0.0f;
            }
        } else if (m_State == State::Walking &&
                   m_Kind == EnemyKind::Koopa &&
                   onGround &&
                   !hasGroundAhead(m_Direction)) {
            m_Direction = -m_Direction;
            m_VelocityX = 0.0f;
        } else {
            m_Transform.translation.x = candidateX;
        }
    }

    RefreshSprite();
}

void Enemy::Stomp() {
    if (!m_Alive) return;

    if (m_Kind == EnemyKind::Koopa) {
        if (m_State == State::Walking || m_State == State::ShellSliding) {
            EnterShellIdle();
        } else {
            m_ShellIdleTimer = KOOPA_SHELL_IDLE_DURATION;
            m_RecoveryTimer = 0.0f;
            m_State = State::ShellIdle;
            m_VelocityX = 0.0f;
            RefreshSprite();
        }
        return;
    }

    m_Alive = false;
    m_FlippedDeath = false;
    m_DeathFinished = false;
    m_VelocityX = 0.0f;
    m_DeathTimer = 0.5f;
    m_Image->SetImage(m_DeathPath);
}

void Enemy::KillFlipped(float horizontalVelocity) {
    m_Alive = false;
    m_FlippedDeath = true;
    m_DeathFinished = false;
    m_DeathTimer = 0.0f;
    m_VelocityX = horizontalVelocity;
    m_VelocityY = FLIPPED_DEATH_LAUNCH_Y;
    m_Transform.scale.x = -std::abs(m_Transform.scale.x);
    m_Transform.scale.y = -std::abs(m_Transform.scale.y);
    m_Image->SetImage(m_Kind == EnemyKind::Koopa ? m_LeftPath : (m_UseLeftWalkFrame ? m_LeftPath : m_RightPath));
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

    if (m_State == State::ShellIdle || m_State == State::ShellSliding) {
        m_Image->SetImage(m_ShellPath);
        return;
    }

    if (m_State == State::Recovering) {
        const bool showWalkSprite = std::fmod(m_RecoveryTimer, 0.18f) < 0.09f;
        if (showWalkSprite) {
            m_Image->SetImage(m_UseLeftWalkFrame ? m_LeftPath : m_RightPath);
        } else {
            m_Image->SetImage(m_ShellPath);
        }
        return;
    }

    m_Image->SetImage(m_UseLeftWalkFrame ? m_LeftPath : m_RightPath);
}
