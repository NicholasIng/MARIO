#include "Enemy.hpp"
#include "EnemyDetail.hpp"

#include "AssetPaths.hpp"
#include "MapManager.hpp"
#include "Util/Time.hpp"
#include <algorithm>
#include <cmath>

using namespace EnemyDetail;

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

