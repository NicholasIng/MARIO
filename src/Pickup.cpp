#include "Pickup.hpp"

#include "Util/Time.hpp"

Pickup::Pickup(LootType type, float x, float y)
    : m_Type(type) {
    const std::string path = (type == LootType::RedMushroom)
        ? "C:\\Users\\asus\\MARIO\\Resources\\image\\Mushroom_red.png"
        : "C:\\Users\\asus\\MARIO\\Resources\\image\\Mushroom_green.png";

    m_Image = std::make_shared<Util::Image>(path);
    SetDrawable(m_Image);
    m_Transform.translation = { x, y };
    m_Transform.scale = { 3.0f, 3.0f };
    m_ZIndex = 8.0f;
}

void Pickup::Update() {
    if (m_Collected) return;

    const float dt = Util::Time::GetDeltaTimeMs() / 1000.0f;
    if (m_RiseRemaining > 0.0f) {
        const float rise = std::min(m_RiseRemaining, 48.0f * dt);
        m_Transform.translation.y += rise;
        m_RiseRemaining -= rise;
    }
}
