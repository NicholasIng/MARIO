#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "MapManager.hpp"
#include "ConvertSketch.hpp"
#include "Enemy.hpp"
#include "config.hpp"
#include <algorithm>

// global pointer for Mario collision
std::unique_ptr<MapManager> g_MapManager;

void App::Start() {
    LOG_TRACE("Start");

    g_MapManager = std::make_unique<MapManager>();
    m_Mario = std::make_unique<Mario>();

    Util::Color bg;
    bool foundSpawn = convert_sketch(
        "C:\\Users\\asus\\MARIO\\Resources\\image\\LevelSketch0.png",
        *g_MapManager,
        *m_Mario,
        bg
    );

    // fallback if red spawn pixel is missing
    if (!foundSpawn) {
        m_Mario->m_Transform.translation = { 0.0f, -200.0f };
    }

    m_ViewX = 0.0f;

    LOG_TRACE("Map size = {} x {}", g_MapManager->GetWidth(), g_MapManager->GetHeight());
    LOG_TRACE("Mario start pos = {}, {}",
        m_Mario->m_Transform.translation.x,
        m_Mario->m_Transform.translation.y);

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    if (m_Mario) {
        m_Mario->Update();
    }

    if (m_Mario && g_MapManager) {
        // follow Mario like the youtuber's i_view_x
        m_ViewX = m_Mario->m_Transform.translation.x;

        // clamp camera so it does not scroll beyond level edges
        float mapLeft = g_MapManager->GetWorldLeft();
        float mapRight = g_MapManager->GetWorldRight();

        float halfScreen = WINDOW_WIDTH / 2.0f;

        float minViewX = mapLeft + halfScreen;
        float maxViewX = mapRight - halfScreen;

        // if level is narrower than the screen, keep camera centered
        if (minViewX > maxViewX) {
            m_ViewX = 0.0f;
        }
        else {
            m_ViewX = std::clamp(m_ViewX, minViewX, maxViewX);
        }
    }

    if (g_MapManager) {
        g_MapManager->Draw(m_ViewX);
    }

    if (m_Mario) {
        // draw Mario relative to camera, but keep real position for physics
        auto oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= m_ViewX;

        m_Mario->Draw();

        m_Mario->m_Transform.translation = oldPos;
    }

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() {
    LOG_TRACE("End");
}