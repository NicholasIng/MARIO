#include "Enemy.hpp"

#include "Util/Time.hpp"

Enemy::Enemy(float x, float y)
    : m_StartX(x),
      m_LeftPath("C:\\Users\\asus\\MARIO\\Resources\\image\\Goomba_l.png"),
      m_RightPath("C:\\Users\\asus\\MARIO\\Resources\\image\\Goomba_r.png") {
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 9.0f;
    RefreshSprite();
}

void Enemy::Update() {
    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;
    m_Transform.translation.x += m_Direction * m_Speed * dt;

    const float patrolHalfWidth = 40.0f;
    if (m_Transform.translation.x < m_StartX - patrolHalfWidth) {
        m_Transform.translation.x = m_StartX - patrolHalfWidth;
        m_Direction = 1.0f;
        RefreshSprite();
    } else if (m_Transform.translation.x > m_StartX + patrolHalfWidth) {
        m_Transform.translation.x = m_StartX + patrolHalfWidth;
        m_Direction = -1.0f;
        RefreshSprite();
    }
}

void Enemy::RefreshSprite() {
    const std::string& path = (m_Direction < 0.0f) ? m_LeftPath : m_RightPath;
    SetDrawable(std::make_unique<Util::Image>(path));
}
