#include "App.hpp"
#include "AppDetail.hpp"
#include "DebugManager.hpp"
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

void App::DrawHud() {
    if (m_HudCoinIcon.object != nullptr) {
        m_HudCoinIcon.object->m_Transform.translation = {
            m_HudCoinValue.position.x - 20.0f,
            m_HudCoinValue.position.y + 2.0f
        };
    }
    DrawSpriteText(m_HudMarioLabel);
    DrawSpriteText(m_HudScoreValue);
    DrawSpriteText(m_HudWorldLabel);
    DrawSpriteText(m_HudWorldValue);
    DrawSpriteText(m_HudTimeLabel);
    DrawSpriteText(m_HudTimeValue);
    DrawUiObject(m_HudCoinIcon.object);
    DrawSpriteText(m_HudCoinValue);
}

void App::RenderTitleScreen() {
    glClearColor(
        m_SkyColor.r / 255.0f,
        m_SkyColor.g / 255.0f,
        m_SkyColor.b / 255.0f,
        1.0f
    );
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_MapManager != nullptr) {
        g_MapManager->Draw(GetLeftEdgeViewX(g_MapManager.get()), 0.0f);
    }

    if (m_Mario != nullptr && g_MapManager != nullptr) {
        const float titleViewX = GetLeftEdgeViewX(g_MapManager.get());
        const glm::vec2 oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= titleViewX;
        m_Mario->m_Transform.translation.y += m_Mario->GetRenderOffsetY();
        m_Mario->Draw();
        m_Mario->m_Transform.translation = oldPos;
    }

    if (m_TitleCoinIcon.object != nullptr) {
        m_TitleCoinIcon.object->m_Transform.translation = {
            m_HudCoinValue.position.x - 20.0f,
            m_HudCoinValue.position.y + 2.0f
        };
    }

    DrawSpriteText(m_HudMarioLabel);
    DrawSpriteText(m_HudScoreValue);
    DrawSpriteText(m_HudWorldLabel);
    DrawSpriteText(m_HudWorldValue);
    DrawSpriteText(m_HudTimeLabel);
    DrawUiObject(m_TitleCoinIcon.object);
    DrawSpriteText(m_HudCoinValue);
    DrawUiObject(m_TitleLogoImage);
    DrawSpriteText(m_TitleOption1);
    DrawSpriteText(m_TitleTopScore);
    DrawUiObject(m_TitleNintendoText);
    DrawUiObject(m_TitleCursor);
    if (m_DebugManager) {
        m_DebugManager->Render(*this);
    }
}

void App::RenderLevelIntro() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (m_HudCoinIcon.object != nullptr) {
        m_HudCoinIcon.object->m_Transform.translation = {
            m_HudCoinValue.position.x - 20.0f,
            m_HudCoinValue.position.y + 2.0f
        };
    }
    DrawSpriteText(m_HudMarioLabel);
    DrawSpriteText(m_HudScoreValue);
    DrawSpriteText(m_HudWorldLabel);
    DrawSpriteText(m_HudWorldValue);
    DrawSpriteText(m_HudTimeLabel);
    DrawUiObject(m_HudCoinIcon.object);
    DrawSpriteText(m_HudCoinValue);
    DrawSpriteText(m_IntroWorldText);
    DrawUiObject(m_IntroMario);
    DrawSpriteText(m_IntroLivesText);
    if (m_DebugManager) {
        m_DebugManager->Render(*this);
    }
}

void App::RenderStatusMessage() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    DrawHud();
    DrawSpriteText(m_StatusMessageText);
    if (m_DebugManager) {
        m_DebugManager->Render(*this);
    }
}

void App::RenderPaused() {
    RenderGameplay();
    DrawSpriteText(m_StatusMessageText);
}

void App::RenderSceneWorld(bool drawCastle, bool drawMarioBehindTiles) {
    glClearColor(m_SkyColor.r / 255.0f, m_SkyColor.g / 255.0f, m_SkyColor.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_MapManager && drawMarioBehindTiles) {
        g_MapManager->DrawBackground(m_ViewX, m_ViewY);
    } else if (g_MapManager) {
        g_MapManager->Draw(m_ViewX, m_ViewY);
    }

    if (drawCastle &&
        m_StartCastleObject != nullptr &&
        g_MapManager != nullptr) {
        const glm::vec2 oldPos = m_StartCastleObject->m_Transform.translation;
        m_StartCastleObject->m_Transform.translation.x -= m_ViewX;
        m_StartCastleObject->m_Transform.translation.y -= m_ViewY;
        m_StartCastleObject->Draw();
        m_StartCastleObject->m_Transform.translation = oldPos;
    }

    if (drawCastle &&
        m_CastleObject != nullptr &&
        g_MapManager != nullptr) {
        const glm::vec2 oldPos = m_CastleObject->m_Transform.translation;
        m_CastleObject->m_Transform.translation.x -= m_ViewX;
        m_CastleObject->m_Transform.translation.y -= m_ViewY;
        m_CastleObject->Draw();
        m_CastleObject->m_Transform.translation = oldPos;
    }
    if (m_Mario) {
        const glm::vec2 oldPos = m_Mario->m_Transform.translation;
        m_Mario->m_Transform.translation.x -= m_ViewX;
        m_Mario->m_Transform.translation.y = m_Mario->m_Transform.translation.y - m_ViewY + m_Mario->GetRenderOffsetY();
        m_Mario->Draw();
        m_Mario->m_Transform.translation = oldPos;
    }

    if (g_MapManager && drawMarioBehindTiles) {
        g_MapManager->DrawTiles(m_ViewX, m_ViewY);
    }

    if (g_MapManager) {
        g_MapManager->DrawForeground(m_ViewX, m_ViewY);
    }
}

void App::RenderGameplay() {
    RenderSceneWorld(true);
    for (auto& enemy : m_Enemies) {
        const glm::vec2 oldPos = enemy->m_Transform.translation;
        enemy->m_Transform.translation.x -= m_ViewX;
        enemy->m_Transform.translation.y -= m_ViewY;
        enemy->Draw();
        enemy->m_Transform.translation = oldPos;
    }
    for (auto& fireball : m_Fireballs) {
        const glm::vec2 oldPos = fireball->m_Transform.translation;
        fireball->m_Transform.translation.x -= m_ViewX;
        fireball->m_Transform.translation.y -= m_ViewY;
        fireball->Draw();
        fireball->m_Transform.translation = oldPos;
    }
    for (auto& pickup : m_Pickups) {
        const glm::vec2 oldPos = pickup->m_Transform.translation;
        pickup->m_Transform.translation.x -= m_ViewX;
        pickup->m_Transform.translation.y -= m_ViewY;
        pickup->Draw();
        pickup->m_Transform.translation = oldPos;
    }
    for (auto& debris : m_Debris) {
        const glm::vec2 oldPos = debris->m_Transform.translation;
        debris->m_Transform.translation.x -= m_ViewX;
        debris->m_Transform.translation.y -= m_ViewY;
        debris->Draw();
        debris->m_Transform.translation = oldPos;
    }

    DrawFloatingTexts();

    DrawHud();
    if (m_DebugManager) {
        m_DebugManager->Render(*this);
    }
}

