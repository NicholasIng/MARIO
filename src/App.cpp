#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "MapManager.hpp"

// Global or member MapManager
std::unique_ptr<MapManager> g_MapManager;

void App::Start() {
    LOG_TRACE("Start");
    
    m_Mario = std::make_unique<Mario>();
    g_MapManager = std::make_unique<MapManager>();

    // Define a simple floor map
    std::vector<std::vector<char>> simpleLevel = {
        {'0', '0', '0', '0', '0'},
        {'0', '?', '0', '?', '0'},
        {'G', 'G', 'G', 'G', 'G'}
    };

    g_MapManager->LoadMap(simpleLevel);
    m_CurrentState = State::UPDATE;
}

void App::Update() {
    // 1. Draw the World first
    if (g_MapManager) {
        g_MapManager->Draw();
    }

    // 2. Update and Draw Mario
    if (m_Mario) {
        m_Mario->Update();
        m_Mario->Draw();
    }

    /* DO NOT TOUCH: Exit Logic */
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() {
    LOG_TRACE("End");
}