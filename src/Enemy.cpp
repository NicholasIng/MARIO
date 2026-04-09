#include "Enemy.hpp"

#include "Util/Time.hpp"

Enemy::Enemy(float x, float y)
    : m_StartX(x),
      m_LeftPath("C:\\Users\\asus\\MARIO\\Resources\\image\\Goomba_l.png"),
      m_RightPath("C:\\Users\\asus\\MARIO\\Resources\\image\\Goomba_r.png"),
      m_DeathPath("C:\\Users\\asus\\MARIO\\Resources\\image\\Goombadeath.png") {
    m_Image = std::make_shared<Util::Image>(m_LeftPath);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 9.0f;
}

void Enemy::Update() {
    if (!m_Alive) {
        m_DeathTimer -= Util::Time::GetDeltaTimeMs() / 1000.0f;
        return;
    }

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

void Enemy::Stomp() {
    m_Alive = false;
    m_DeathTimer = 0.5f;
    m_Image->SetImage(m_DeathPath);
}

void Enemy::RefreshSprite() {
    const std::string& path = (m_Direction < 0.0f) ? m_LeftPath : m_RightPath;
    m_Image->SetImage(path);
}
