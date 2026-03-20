#include "Mario.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include <algorithm>
#include <cmath>

Mario::Mario()
    : m_VelocityX(0.0f), m_VelocityY(0.0f), m_Acceleration(1500.0f),
    m_MaxSpeed(350.0f), m_Friction(1000.0f), m_Gravity(-2200.0f),
    m_JumpForce(600.0f), m_OnGround(false), m_JumpTimer(0.0f), m_MaxJumpTime(0.25f) {

    // Define Animations
    m_Animations[AnimState::IDLE] = std::make_unique<Animation>(
        std::vector<std::string>{RESOURCE_DIR "/Image/Character/MarioIdle.png"}, 1.0f);

    m_Animations[AnimState::WALK] = std::make_unique<Animation>(
        std::vector<std::string>{
        RESOURCE_DIR "/Image/Character/MarioWalk1.png",
            RESOURCE_DIR "/Image/Character/MarioWalk2.png",
            RESOURCE_DIR "/Image/Character/MarioWalk3.png"
    }, 0.07f);

    m_Animations[AnimState::JUMP] = std::make_unique<Animation>(
        std::vector<std::string>{RESOURCE_DIR "/Image/Character/MarioJump.png"}, 1.0f);

    m_Animations[AnimState::BRAKE] = std::make_unique<Animation>(
        std::vector<std::string>{RESOURCE_DIR "/Image/Character/MarioBrake.png"}, 1.0f);

    // Added Crouch Animation (usually a single frame)
    m_Animations[AnimState::CROUCH] = std::make_unique<Animation>(
        std::vector<std::string>{RESOURCE_DIR "/Image/Character/MarioCrouch.png"}, 1.0f);

    SetDrawable(std::make_unique<Util::Image>(m_Animations[AnimState::IDLE]->GetCurrentFramePath()));

    m_Transform.translation = { 0.0f, -250.0f };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 10.0f;
}

void Mario::Update() {
    float dt = 0.016f;

    // --- Input Checks ---
    bool moveLeft = Util::Input::IsKeyPressed(Util::Keycode::A);
    bool moveRight = Util::Input::IsKeyPressed(Util::Keycode::D);
    // Check for 'S' key to crouch
    m_IsCrouching = Util::Input::IsKeyPressed(Util::Keycode::S) && m_OnGround;

    // --- Horizontal Movement ---
    // Mario cannot walk while crouching in the original game
    bool isMoving = false;
    if (!m_IsCrouching) {
        if (moveLeft && !moveRight) {
            m_VelocityX -= m_Acceleration * dt;
            isMoving = true;
            if (m_VelocityX <= 0) m_Transform.scale.x = -std::abs(m_Transform.scale.x);
        }
        else if (moveRight && !moveLeft) {
            m_VelocityX += m_Acceleration * dt;
            isMoving = true;
            if (m_VelocityX >= 0) m_Transform.scale.x = std::abs(m_Transform.scale.x);
        }
    }

    // Apply Friction (Always apply friction if not actively moving, including when crouching)
    if (!isMoving && m_OnGround) {
        if (m_VelocityX > 0) m_VelocityX = std::max(0.0f, m_VelocityX - m_Friction * dt);
        else if (m_VelocityX < 0) m_VelocityX = std::min(0.0f, m_VelocityX + m_Friction * dt);
    }
    m_VelocityX = std::clamp(m_VelocityX, -m_MaxSpeed, m_MaxSpeed);

    // --- Vertical Movement ---
    // You cannot jump while crouching
    if (!m_IsCrouching && Util::Input::IsKeyDown(Util::Keycode::SPACE) && m_OnGround) {
        m_VelocityY = m_JumpForce;
        m_OnGround = false;
        m_JumpTimer = m_MaxJumpTime;
    }

    if (Util::Input::IsKeyPressed(Util::Keycode::SPACE) && m_JumpTimer > 0.0f) {
        m_VelocityY = m_JumpForce;
        m_JumpTimer -= dt;
    }
    else { m_JumpTimer = 0.0f; }

    m_VelocityY += m_Gravity * dt;
    m_Transform.translation.x += m_VelocityX * dt;
    m_Transform.translation.y += m_VelocityY * dt;

    // Ground Collision Placeholder
    if (m_Transform.translation.y < -250.0f) {
        m_Transform.translation.y = -250.0f;
        m_VelocityY = 0.0f;
        m_OnGround = true;
    }

    HandleAnimation(dt);
}

void Mario::HandleAnimation(float dt) {
    AnimState lastState = m_AnimState;

    if (!m_OnGround) {
        m_AnimState = AnimState::JUMP;
    }
    else if (m_IsCrouching) {
        m_AnimState = AnimState::CROUCH;
    }
    else if (std::abs(m_VelocityX) > 20.0f) {
        if (m_VelocityX > 100.0f && Util::Input::IsKeyPressed(Util::Keycode::A)) {
            m_AnimState = AnimState::BRAKE;
            m_Transform.scale.x = std::abs(m_Transform.scale.x);
        }
        else if (m_VelocityX < -100.0f && Util::Input::IsKeyPressed(Util::Keycode::D)) {
            m_AnimState = AnimState::BRAKE;
            m_Transform.scale.x = -std::abs(m_Transform.scale.x);
        }
        else {
            m_AnimState = AnimState::WALK;
        }
    }
    else {
        m_AnimState = AnimState::IDLE;
    }

    if (lastState != m_AnimState) {
        m_Animations[m_AnimState]->Reset();
    }

    m_Animations[m_AnimState]->Update(dt);
    SetDrawable(std::make_unique<Util::Image>(m_Animations[m_AnimState]->GetCurrentFramePath()));
}