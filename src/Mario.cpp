#include "Mario.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include <algorithm>
#include <cmath>

extern std::unique_ptr<MapManager> g_MapManager;

Mario::Mario()
    : m_VelocityX(0.0f),
    m_VelocityY(0.0f),
    m_Acceleration(1500.0f),
    m_MaxSpeed(350.0f),
    m_Friction(1000.0f),
    m_Gravity(-2200.0f),
    m_JumpForce(700.0f),
    m_OnGround(false),
    m_JumpTimer(0.0f),
    m_MaxJumpTime(0.25f) {

    m_Animations[AnimState::IDLE] = std::make_unique<Animation>(
        std::vector<std::string>{
        "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioIdle.png"
    }, 1.0f
    );

    m_Animations[AnimState::WALK] = std::make_unique<Animation>(
        std::vector<std::string>{
        "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioWalk1.png",
            "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioWalk2.png",
            "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioWalk3.png"
    }, 0.07f
    );

    m_Animations[AnimState::JUMP] = std::make_unique<Animation>(
        std::vector<std::string>{
        "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioJump.png"
    }, 1.0f
    );

    m_Animations[AnimState::BRAKE] = std::make_unique<Animation>(
        std::vector<std::string>{
        "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioBrake.png"
    }, 1.0f
    );

    m_Animations[AnimState::CROUCH] = std::make_unique<Animation>(
        std::vector<std::string>{
        "C:\\Users\\asus\\MARIO\\Resources\\image\\Character\\MarioCrouch.png"
    }, 1.0f
    );

    m_Image = std::make_shared<Util::Image>(
        m_Animations[AnimState::IDLE]->GetCurrentFramePath()
    );
    SetDrawable(m_Image);

    m_Transform.translation = m_SpawnPosition;
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 10.0f;
}

void Mario::Update() {
    float dt = std::max(0.001f, Util::Time::GetDeltaTimeMs() / 1000.0f);

    if (m_IsDead) {
        m_RespawnTimer -= dt;
        if (m_RespawnTimer <= 0.0f) {
            m_IsDead = false;
            m_VelocityX = 0.0f;
            m_VelocityY = 0.0f;
            m_OnGround = false;
            m_JumpTimer = 0.0f;
            m_Transform.translation = m_SpawnPosition;
        }
        return;
    }

    bool moveLeft = Util::Input::IsKeyPressed(Util::Keycode::A);
    bool moveRight = Util::Input::IsKeyPressed(Util::Keycode::D);
    m_IsCrouching = Util::Input::IsKeyPressed(Util::Keycode::S) && m_OnGround;

    bool isMoving = false;
    if (!m_IsCrouching) {
        if (moveLeft && !moveRight) {
            m_VelocityX -= m_Acceleration * dt;
            isMoving = true;
            m_Transform.scale.x = -std::abs(m_Transform.scale.x);
        }
        else if (moveRight && !moveLeft) {
            m_VelocityX += m_Acceleration * dt;
            isMoving = true;
            m_Transform.scale.x = std::abs(m_Transform.scale.x);
        }
    }

    if (!isMoving && m_OnGround) {
        if (m_VelocityX > 0) m_VelocityX = std::max(0.0f, m_VelocityX - m_Friction * dt);
        else if (m_VelocityX < 0) m_VelocityX = std::min(0.0f, m_VelocityX + m_Friction * dt);
    }

    m_VelocityX = std::clamp(m_VelocityX, -m_MaxSpeed, m_MaxSpeed);

    if (!m_IsCrouching && Util::Input::IsKeyDown(Util::Keycode::SPACE) && m_OnGround) {
        m_VelocityY = m_JumpForce;
        m_OnGround = false;
        m_JumpTimer = m_MaxJumpTime;
    }

    if (Util::Input::IsKeyPressed(Util::Keycode::SPACE) && m_JumpTimer > 0.0f) {
        m_VelocityY = m_JumpForce;
        m_JumpTimer -= dt;
    }
    else {
        m_JumpTimer = 0.0f;
    }

    m_VelocityY += m_Gravity * dt;

    float moveX = m_VelocityX * dt;
    float moveY = m_VelocityY * dt;

    // axis-separated collision resolution:
    // - first resolve horizontal movement (with current Y)
    // - then resolve vertical movement (with resolved X)

    m_OnGround = false;

    // use map tile size and geometry from map manager if available
    float tileSize = 48.0f;
    int mapWidth = 0;
    int mapHeight = 0;
    float mapLeft = 0.0f;
    float mapTop = 0.0f;
    if (g_MapManager) {
        tileSize = g_MapManager->GetTileSize();
        mapWidth = g_MapManager->GetWidth();
        mapHeight = g_MapManager->GetHeight();
        mapLeft = -(mapWidth * tileSize) / 2.0f;
        mapTop = (mapHeight * tileSize) / 2.0f;
    }

    // player extents (half sizes) in world units
    float halfWidth = 20.0f;
    float halfHeight = 24.0f;

    const float eps = 0.001f;

    float curX = m_Transform.translation.x;
    float curY = m_Transform.translation.y;

    float candidateX = curX;
    float candidateY = curY;

    // --- HORIZONTAL ---
    if (g_MapManager && std::abs(moveX) > 0.0f) {
        candidateX = curX + moveX;
        // sample vertically between top and bottom
        float topY = curY + halfHeight;
        float bottomY = curY - halfHeight;

        int topGridY = static_cast<int>(std::floor((mapTop - topY) / tileSize));
        int bottomGridY = static_cast<int>(std::floor((mapTop - bottomY) / tileSize));

        topGridY = std::max(0, std::min(mapHeight - 1, topGridY));
        bottomGridY = std::max(0, std::min(mapHeight - 1, bottomGridY));

        if (moveX > 0.0f) {
            float rightEdge = candidateX + halfWidth;
            int gridX = static_cast<int>(std::floor((rightEdge - mapLeft - eps) / tileSize));
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                if (g_MapManager->GetCell(gridX, gy) != Cell::Empty) {
                    // collide with tile at (gridX, gy)
                    float tileLeft = mapLeft + gridX * tileSize;
                    candidateX = tileLeft - halfWidth - eps;
                    m_VelocityX = 0.0f;
                    break;
                }
            }
        }
        else { // moving left
            float leftEdge = candidateX - halfWidth;
            int gridX = static_cast<int>(std::floor((leftEdge - mapLeft + eps) / tileSize));
            for (int gy = topGridY; gy <= bottomGridY; ++gy) {
                if (g_MapManager->GetCell(gridX, gy) != Cell::Empty) {
                    float tileRight = mapLeft + (gridX + 1) * tileSize;
                    candidateX = tileRight + halfWidth + eps;
                    m_VelocityX = 0.0f;
                    break;
                }
            }
        }
    }
    else {
        candidateX = curX + moveX;
    }

    // --- VERTICAL ---
    if (g_MapManager && std::abs(moveY) > 0.0f) {
        candidateY = curY + moveY;

        float leftX = candidateX - halfWidth;
        float rightX = candidateX + halfWidth;

        int leftGridX = static_cast<int>(std::floor((leftX - mapLeft + eps) / tileSize));
        int rightGridX = static_cast<int>(std::floor((rightX - mapLeft - eps) / tileSize));

        leftGridX = std::max(0, std::min(mapWidth - 1, leftGridX));
        rightGridX = std::max(0, std::min(mapWidth - 1, rightGridX));

        if (moveY > 0.0f) { // moving up (jump)
            float topEdge = candidateY + halfHeight;
            int gridY = static_cast<int>(std::floor((mapTop - topEdge + eps) / tileSize));
            gridY = std::max(0, std::min(mapHeight - 1, gridY));
            for (int gx = leftGridX; gx <= rightGridX; ++gx) {
                if (g_MapManager->GetCell(gx, gridY) != Cell::Empty) {
                    if (g_MapManager->GetCell(gx, gridY) == Cell::QuestionBlock) {
                        g_MapManager->HitQuestionBlock(gx, gridY);
                    }
                    // collide with ceiling of tile at (gx, gridY)
                    float tileBottom = mapTop - (gridY + 1) * tileSize;
                    candidateY = tileBottom - halfHeight - eps;
                    m_VelocityY = 0.0f;
                    break;
                }
            }
        }
        else { // moving down (fall)
            float bottomEdge = candidateY - halfHeight;
            int gridY = static_cast<int>(std::floor((mapTop - bottomEdge - eps) / tileSize));
            gridY = std::max(0, std::min(mapHeight - 1, gridY));
            for (int gx = leftGridX; gx <= rightGridX; ++gx) {
                if (g_MapManager->GetCell(gx, gridY) != Cell::Empty) {
                    // collide with top of tile at (gx, gridY)
                    float tileTop = mapTop - gridY * tileSize;
                    candidateY = tileTop + halfHeight + eps;
                    m_VelocityY = 0.0f;
                    m_OnGround = true;
                    break;
                }
            }
        }
    }
    else {
        candidateY = curY + moveY;
    }

    // Apply resolved candidate position
    m_Transform.translation.x = candidateX;
    m_Transform.translation.y = candidateY;

    HandleAnimation(dt);
}

void Mario::Die() {
    if (m_IsDead) return;
    m_IsDead = true;
    m_RespawnTimer = 1.0f;
    m_VelocityX = 0.0f;
    m_VelocityY = 0.0f;
}

void Mario::BounceAfterStomp() {
    m_VelocityY = m_JumpForce * 0.6f;
    m_OnGround = false;
    m_JumpTimer = 0.0f;
}

void Mario::HandleAnimation(float dt) {
    AnimState lastState = m_AnimState;

    if (!m_OnGround) m_AnimState = AnimState::JUMP;
    else if (m_IsCrouching) m_AnimState = AnimState::CROUCH;
    else if (std::abs(m_VelocityX) > 20.0f) {
        if (m_VelocityX > 100.0f && Util::Input::IsKeyPressed(Util::Keycode::A))
            m_AnimState = AnimState::BRAKE;
        else if (m_VelocityX < -100.0f && Util::Input::IsKeyPressed(Util::Keycode::D))
            m_AnimState = AnimState::BRAKE;
        else
            m_AnimState = AnimState::WALK;
    }
    else {
        m_AnimState = AnimState::IDLE;
    }

    if (lastState != m_AnimState) {
        m_Animations[m_AnimState]->Reset();
    }

    m_Animations[m_AnimState]->Update(dt);

    m_Image->SetImage(m_Animations[m_AnimState]->GetCurrentFramePath());
}
