#include "App.hpp"
#include "AppDetail.hpp"
#include "Util/BGM.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/SFX.hpp"
#include "Util/Time.hpp"
#include "MapManager.hpp"
#include "ConvertSketch.hpp"
#include "Enemy.hpp"
#include "AssetPaths.hpp"
#include "config.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

using namespace AppDetail;

namespace {
constexpr bool DEBUG_START_AT_LEVEL_ONE_ONE_TO_TWO_TRANSITION = true;
}

void App::Start() {
    LOG_TRACE("Start");

    InitializeAudio();
    ResetGameSession();
    if (DEBUG_START_AT_LEVEL_ONE_ONE_TO_TWO_TRANSITION) {
        LoadLevel();
        LoadTransitionScene();
    } else {
        LoadLevel();
        m_ScreenState = ScreenState::Title;
        PlayTitleMusic();
    }

    LOG_TRACE("Map size = {} x {}", g_MapManager->GetWidth(), g_MapManager->GetHeight());
    LOG_TRACE("Mario start pos = {}, {}",
        m_Mario->m_Transform.translation.x,
        m_Mario->m_Transform.translation.y);

    m_CurrentState = State::UPDATE;
}

void App::End() {
    LOG_TRACE("End");
}
