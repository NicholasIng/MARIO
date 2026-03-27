#include "Mario.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
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

    m_Image = std::make_unique<Util::Image>(
        m_Animations[AnimState::IDLE]->GetCurrentFramePath()
    );
    SetDrawable(std::move(m_Image));

    m_Transform.translation = { 0.0f, -250.0f };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 10.0f;
}

void Mario::Update() {
    float dt = 0.016f;

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

    m_OnGround = false;

    float tileSize = 48.0f;
    float halfWidth = 20.0f;
    float halfHeight = 24.0f;

    if (g_MapManager) {
        int mapWidth = g_MapManager->GetWidth();
        int mapHeight = g_MapManager->GetHeight();

        float mapLeft = -(mapWidth * tileSize) / 2.0f;
        float mapTop = (mapHeight * tileSize) / 2.0f;

        float nextX = m_Transform.translation.x + moveX;
        float nextY = m_Transform.translation.y + moveY;

        float leftFootX = nextX - halfWidth;
        float rightFootX = nextX + halfWidth;
        float feetY = nextY - halfHeight;

        int gridLeftX = (int)((leftFootX - mapLeft) / tileSize);
        int gridRightX = (int)((rightFootX - mapLeft) / tileSize);
        int gridY = (int)((mapTop - feetY) / tileSize);

        if (g_MapManager->GetCell(gridLeftX, gridY) != Cell::Empty ||
            g_MapManager->GetCell(gridRightX, gridY) != Cell::Empty) {

            float tileTop = mapTop - gridY * tileSize;

            m_Transform.translation.y = tileTop + halfHeight;
            m_VelocityY = 0.0f;
            moveY = 0.0f;
            m_OnGround = true;
        }
    }

    m_Transform.translation.x += moveX;
    m_Transform.translation.y += moveY;

    HandleAnimation(dt);
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

    // TEMP: keep current drawable stable for now
    SetDrawable(std::make_unique<Util::Image>(
        m_Animations[m_AnimState]->GetCurrentFramePath()
    ));
}