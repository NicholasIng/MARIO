#include "DebugManager.hpp"

#include "App.hpp"
#include "AssetPaths.hpp"
#include "Enemy.hpp"
#include "GameImage.hpp"
#include "MapManager.hpp"
#include "Pickup.hpp"
#include "Util/GameObject.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Time.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>

extern std::unique_ptr<MapManager> g_MapManager;

namespace {
constexpr bool kSubmissionDebugEnabled = true;

std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

bool IsOnScreen(const glm::vec2& center, const glm::vec2& halfExtents) {
    const float halfScreenWidth = static_cast<float>(WINDOW_WIDTH) * 0.5f + 64.0f;
    const float halfScreenHeight = static_cast<float>(WINDOW_HEIGHT) * 0.5f + 64.0f;
    return center.x + halfExtents.x >= -halfScreenWidth &&
           center.x - halfExtents.x <= halfScreenWidth &&
           center.y + halfExtents.y >= -halfScreenHeight &&
           center.y - halfExtents.y <= halfScreenHeight;
}
}

DebugManager::DebugManager()
    : m_DebugPixel(std::make_shared<GameImage>(AssetPaths::Image("dot.png"))) {
}

bool DebugManager::HasFlag(Flag flag) const {
    return (m_Flags & static_cast<std::uint32_t>(flag)) != 0u;
}

void DebugManager::SetFlag(Flag flag, bool enabled) {
    if (enabled) {
        m_Flags |= static_cast<std::uint32_t>(flag);
    } else {
        m_Flags &= ~static_cast<std::uint32_t>(flag);
    }
}

void DebugManager::ToggleFlag(Flag flag) {
    SetFlag(flag, !HasFlag(flag));
}

void DebugManager::SyncMarioDebugFlags(App& app) const {
    if (!app.m_Mario) {
        return;
    }

    app.m_Mario->SetDebugGodMode(IsGodModeEnabled());
    app.m_Mario->SetDebugFlyMode(IsFlyModeEnabled());
    app.m_Mario->SetDebugNoclip(false);
}

void DebugManager::HandleHotkeys(App& app, float dt) {
    if (!kSubmissionDebugEnabled) {
        (void)dt;
        SetFlag(Flag::Overlay, false);
        SetFlag(Flag::Hitboxes, false);
        SetFlag(Flag::GodMode, false);
        SetFlag(Flag::FreeCamera, false);
        SetFlag(Flag::FlyMode, false);
        SetFlag(Flag::WarpMenu, false);
        SetFlag(Flag::Noclip, false);
        SyncMarioDebugFlags(app);
        return;
    }

    m_LastFps = 1.0f / std::max(dt, 0.0001f);
    bool toggledWarpMenuThisFrame = false;

    if (Util::Input::IsKeyDown(Util::Keycode::F1) &&
        (app.m_ScreenState == App::ScreenState::Gameplay ||
         app.m_ScreenState == App::ScreenState::Paused)) {
        ToggleFlag(Flag::Overlay);
        if (!IsOverlayEnabled()) {
            SetFlag(Flag::Hitboxes, false);
            SetFlag(Flag::GodMode, false);
            SetFlag(Flag::FreeCamera, false);
            SetFlag(Flag::FlyMode, false);
            SetFlag(Flag::WarpMenu, false);
            SetFlag(Flag::Noclip, false);
            app.m_ViewY = 0.0f;
        }
        SyncMarioDebugFlags(app);
    }

    if (!IsOverlayEnabled()) {
        SyncMarioDebugFlags(app);
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::F2)) {
        ToggleFlag(Flag::GodMode);
    }
    if (Util::Input::IsKeyDown(Util::Keycode::F3)) {
        ToggleFlag(Flag::FreeCamera);
        if (!IsFreeCameraEnabled()) {
            app.m_ViewY = 0.0f;
        }
    }
    if (Util::Input::IsKeyDown(Util::Keycode::F4) && app.m_Mario) {
        app.m_Mario->CyclePowerState();
    }
    if (Util::Input::IsKeyDown(Util::Keycode::F5)) {
        ToggleFlag(Flag::FlyMode);
    }
    if (Util::Input::IsKeyDown(Util::Keycode::F6) && app.m_Mario) {
        SpawnMushroomNearMario(app);
    }
    if (Util::Input::IsKeyDown(Util::Keycode::F7) && app.m_Mario) {
        SpawnStarNearMario(app);
    }
    if (Util::Input::IsKeyDown(Util::Keycode::F8) &&
        (app.m_ScreenState == App::ScreenState::Gameplay ||
         app.m_ScreenState == App::ScreenState::Paused)) {
        ToggleFlag(Flag::WarpMenu);
        toggledWarpMenuThisFrame = true;
    }
    SyncMarioDebugFlags(app);

    if (IsWarpMenuOpen() && !toggledWarpMenuThisFrame) {
        HandleWarpMenuInput(app);
    }
    if (IsFreeCameraEnabled() &&
        (app.m_ScreenState == App::ScreenState::Gameplay ||
         app.m_ScreenState == App::ScreenState::Paused)) {
        UpdateFreeCamera(app, dt);
    }
}

void DebugManager::UpdateFreeCamera(App& app, float dt) {
    if (!g_MapManager) {
        return;
    }

    glm::vec2 cameraInput(0.0f, 0.0f);
    if (Util::Input::IsKeyPressed(Util::Keycode::LEFT)) cameraInput.x -= 1.0f;
    if (Util::Input::IsKeyPressed(Util::Keycode::RIGHT)) cameraInput.x += 1.0f;
    if (Util::Input::IsKeyPressed(Util::Keycode::UP)) cameraInput.y += 1.0f;
    if (Util::Input::IsKeyPressed(Util::Keycode::DOWN)) cameraInput.y -= 1.0f;

    if (glm::length(cameraInput) > 0.0f) {
        cameraInput = glm::normalize(cameraInput);
    }

    app.m_ViewX += cameraInput.x * m_FreeCameraSpeed * dt;
    app.m_ViewY += cameraInput.y * m_FreeCameraSpeed * dt;

    if (!g_MapManager) {
        return;
    }

    const float halfScreenWidth = static_cast<float>(WINDOW_WIDTH) * 0.5f;
    const float halfScreenHeight = static_cast<float>(WINDOW_HEIGHT) * 0.5f;
    const float minViewX = g_MapManager->GetWorldLeft() + halfScreenWidth;
    const float maxViewX = g_MapManager->GetWorldRight() - halfScreenWidth;
    if (minViewX > maxViewX) {
        app.m_ViewX = 0.0f;
    } else {
        app.m_ViewX = std::clamp(app.m_ViewX, minViewX, maxViewX);
    }

    const float mapTop = g_MapManager->GetHeight() * g_MapManager->GetTileSize() * 0.5f;
    const float mapBottom = -mapTop;
    const float minViewY = mapBottom + halfScreenHeight;
    const float maxViewY = mapTop - halfScreenHeight;
    if (minViewY > maxViewY) {
        app.m_ViewY = 0.0f;
    } else {
        app.m_ViewY = std::clamp(app.m_ViewY, minViewY, maxViewY);
    }
}

void DebugManager::HandleWarpMenuInput(App& app) {
    if (Util::Input::IsKeyDown(Util::Keycode::F8)) {
        SetFlag(Flag::WarpMenu, false);
        return;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::UP)) {
        m_WarpMenuIndex = (m_WarpMenuIndex + 2) % 3;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::DOWN)) {
        m_WarpMenuIndex = (m_WarpMenuIndex + 1) % 3;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::RETURN) ||
        Util::Input::IsKeyDown(Util::Keycode::SPACE)) {
        WarpToSelectedLevel(app);
    }
}

void DebugManager::WarpToSelectedLevel(App& app) {
    switch (m_WarpMenuIndex) {
    case 0:
        app.LoadLevel(true);
        break;
    case 1:
        app.LoadLevelOneTwo(true, false);
        break;
    case 2:
        app.LoadLevelOneThree(true);
        break;
    default:
        break;
    }

    app.m_ScreenState = App::ScreenState::Gameplay;
    app.m_GoalSequenceStage = App::GoalSequenceStage::None;
    app.m_TransitionPipeReached = false;
    app.m_TransitionMarioHidden = false;
    app.m_TransitionAutoWalkStarted = false;
    app.m_WasMarioDead = false;
    if (app.m_Mario) {
        app.m_Mario->SetVisible(true);
    }
    app.PlayGameplayMusic(true);
    SetFlag(Flag::WarpMenu, false);
}

void DebugManager::SpawnGoombaAtMario(App& app) {
    if (!app.m_Mario) {
        return;
    }

    app.m_Enemies.push_back(std::make_unique<Enemy>(
        app.m_Mario->m_Transform.translation.x,
        app.m_Mario->m_Transform.translation.y,
        EnemyKind::Goomba
    ));
}

void DebugManager::SpawnMushroomNearMario(App& app) {
    if (!app.m_Mario) {
        return;
    }

    const float tileSize = g_MapManager ? g_MapManager->GetTileSize() : 48.0f;
    app.m_Pickups.push_back(std::make_unique<Pickup>(
        LootType::RedMushroom,
        app.m_Mario->m_Transform.translation.x + tileSize,
        app.m_Mario->m_Transform.translation.y
    ));
}

void DebugManager::SpawnStarNearMario(App& app) {
    if (!app.m_Mario) {
        return;
    }

    const float tileSize = g_MapManager ? g_MapManager->GetTileSize() : 48.0f;
    app.m_Pickups.push_back(std::make_unique<Pickup>(
        LootType::Star,
        app.m_Mario->m_Transform.translation.x + tileSize,
        app.m_Mario->m_Transform.translation.y
    ));
}

void DebugManager::Render(App& app) {
    if (!kSubmissionDebugEnabled) {
        (void)app;
        return;
    }

    if (AreHitboxesEnabled()) {
        DrawHitboxes(app);
    }
    if (IsOverlayEnabled()) {
        DrawOverlay(app);
    }
    if (IsWarpMenuOpen()) {
        DrawWarpMenu(app);
    }
}

std::string DebugManager::FontPathForChar(char character) const {
    switch (character) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return AssetPaths::Image(std::string("Font/") + character + ".png");
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
        return AssetPaths::Image(std::string("Font/") + character + ".png");
    case '-':
        return AssetPaths::Image("Font/strip.png");
    case '.':
        return AssetPaths::Image("Font/dot.png");
    case '!':
        return AssetPaths::Image("Font/exclamation.png");
    case 'x':
        return AssetPaths::Image("Font/times.png");
    default:
        return "";
    }
}

void DebugManager::SetTextLine(std::size_t index,
                               const std::string& text,
                               const glm::vec2& position,
                               const glm::vec2& scale,
                               float spacing,
                               float zIndex) {
    if (index >= m_TextLines.size()) {
        m_TextLines.resize(index + 1);
    }

    TextLine& line = m_TextLines[index];
    line.value = text;
    line.glyphs.clear();

    const float glyphWidth = 16.0f * scale.x;
    float cursorX = position.x;
    for (char rawCharacter : text) {
        if (rawCharacter == ' ') {
            cursorX += glyphWidth + spacing;
            continue;
        }

        char lookup = rawCharacter;
        if (lookup != 'x') {
            lookup = static_cast<char>(std::toupper(static_cast<unsigned char>(lookup)));
        }

        const std::string spritePath = FontPathForChar(lookup);
        if (spritePath.empty()) {
            cursorX += glyphWidth + spacing;
            continue;
        }

        auto glyph = std::make_shared<Util::GameObject>();
        glyph->SetDrawable(std::make_shared<GameImage>(spritePath));
        glyph->m_Transform.translation = { cursorX + glyphWidth * 0.5f, position.y };
        glyph->m_Transform.scale = scale;
        glyph->SetZIndex(zIndex);
        line.glyphs.push_back(glyph);
        cursorX += glyphWidth + spacing;
    }
}

void DebugManager::DrawTextLine(const TextLine& line) const {
    for (const auto& glyph : line.glyphs) {
        glyph->Draw();
    }
}

void DebugManager::DrawOverlay(App& app) {
    if (!app.m_Mario) {
        return;
    }

    const int activeEntityCount =
        1 +
        static_cast<int>(app.m_Enemies.size()) +
        static_cast<int>(app.m_Pickups.size()) +
        static_cast<int>(app.m_Fireballs.size()) +
        static_cast<int>(app.m_Debris.size());

    const glm::vec2 marioPosition = app.m_Mario->m_Transform.translation;
    const glm::vec2 marioVelocity = app.m_Mario->GetVelocity();

    std::vector<std::string> lines = {
        "DEBUG MODE",
        "FPS " + std::to_string(static_cast<int>(std::round(m_LastFps))),
        "LEVEL " + std::to_string(app.m_World) + "-" + std::to_string(app.m_Level),
        "MARIO",
        "VEL " + FormatFloat(marioVelocity.x) + " " + FormatFloat(marioVelocity.y),
        "FORM " + app.m_Mario->GetDebugPowerStateName(),
        "STATE " + app.m_Mario->GetDebugStateName(),
        "GOD MODE " + std::string(IsGodModeEnabled() ? "ON" : "OFF"),
        "FLY MODE " + std::string(IsFlyModeEnabled() ? "ON" : "OFF"),
        "FREECAM " + std::string(IsFreeCameraEnabled() ? "ON" : "OFF"),
        "ENTITIES " + std::to_string(activeEntityCount),
        "F1 OVERLAY",
        "F2 GOD",
        "F3 FREECAM",
        "F4 POWER",
        "F5 FLY",
        "F6 MUSHROOM",
        "F7 STAR",
        "F8 WARP",
        "WASD FLY",
        "ARROWS CAM"
    };

    const glm::vec2 origin(-372.0f, 252.0f);
    const float lineHeight = 18.0f;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        SetTextLine(i, lines[i], { origin.x, origin.y - static_cast<float>(i) * lineHeight }, { 1.1f, 1.1f }, 0.0f, 90.0f);
        DrawTextLine(m_TextLines[i]);
    }
}

DebugManager::BoxPrimitive& DebugManager::AcquireBox(std::size_t index) {
    if (index >= m_BoxPool.size()) {
        BoxPrimitive primitive;
        primitive.top = std::make_shared<Util::GameObject>();
        primitive.bottom = std::make_shared<Util::GameObject>();
        primitive.left = std::make_shared<Util::GameObject>();
        primitive.right = std::make_shared<Util::GameObject>();
        primitive.top->SetDrawable(m_DebugPixel);
        primitive.bottom->SetDrawable(m_DebugPixel);
        primitive.left->SetDrawable(m_DebugPixel);
        primitive.right->SetDrawable(m_DebugPixel);
        m_BoxPool.push_back(primitive);
    }
    return m_BoxPool[index];
}

void DebugManager::DrawOutlineBox(std::size_t& boxIndex,
                                  const glm::vec2& center,
                                  const glm::vec2& halfExtents,
                                  float viewX,
                                  float viewY,
                                  float zIndex) {
    const glm::vec2 screenCenter = { center.x - viewX, center.y - viewY };
    if (!IsOnScreen(screenCenter, halfExtents)) {
        return;
    }

    BoxPrimitive& box = AcquireBox(boxIndex++);
    const glm::vec2 pixelSize = m_DebugPixel->GetSize();
    const float pixelWidth = std::max(pixelSize.x, 1.0f);
    const float pixelHeight = std::max(pixelSize.y, 1.0f);
    const float thickness = 2.0f;
    const float width = halfExtents.x * 2.0f;
    const float height = halfExtents.y * 2.0f;

    auto configureBar = [&](const std::shared_ptr<Util::GameObject>& bar,
                            const glm::vec2& translation,
                            const glm::vec2& size) {
        bar->m_Transform.translation = translation;
        bar->m_Transform.scale = { size.x / pixelWidth, size.y / pixelHeight };
        bar->SetZIndex(zIndex);
        bar->Draw();
    };

    configureBar(box.top, { screenCenter.x, screenCenter.y + halfExtents.y }, { width, thickness });
    configureBar(box.bottom, { screenCenter.x, screenCenter.y - halfExtents.y }, { width, thickness });
    configureBar(box.left, { screenCenter.x - halfExtents.x, screenCenter.y }, { thickness, height });
    configureBar(box.right, { screenCenter.x + halfExtents.x, screenCenter.y }, { thickness, height });
}

void DebugManager::DrawHitboxes(App& app) {
    std::size_t boxIndex = 0;
    const float viewX = app.m_ViewX;
    const float viewY = app.m_ViewY;

    if (app.m_Mario) {
        DrawOutlineBox(boxIndex, app.m_Mario->m_Transform.translation, app.m_Mario->GetHalfExtents(), viewX, viewY, 85.0f);
    }

    for (const auto& enemy : app.m_Enemies) {
        DrawOutlineBox(boxIndex, enemy->m_Transform.translation, enemy->GetHalfExtents(), viewX, viewY, 84.0f);
    }

    for (const auto& pickup : app.m_Pickups) {
        DrawOutlineBox(boxIndex, pickup->m_Transform.translation, pickup->GetHalfExtents(), viewX, viewY, 84.0f);
    }

    for (const auto& fireball : app.m_Fireballs) {
        DrawOutlineBox(boxIndex, fireball->m_Transform.translation, fireball->GetHalfExtents(), viewX, viewY, 84.0f);
    }

    if (g_MapManager) {
        for (const auto& tile : g_MapManager->GetSolidCollisionBoxes()) {
            DrawOutlineBox(boxIndex, tile.center, tile.halfExtents, viewX, viewY, 83.0f);
        }
        for (const auto& platform : g_MapManager->GetMovingPlatformCollisionBoxes()) {
            DrawOutlineBox(boxIndex, platform.center, platform.halfExtents, viewX, viewY, 83.0f);
        }
    }
}

void DebugManager::DrawWarpMenu(App&) {
    const std::vector<std::string> lines = {
        "WARP MENU",
        std::string(m_WarpMenuIndex == 0 ? "! " : "  ") + "WORLD 1-1",
        std::string(m_WarpMenuIndex == 1 ? "! " : "  ") + "WORLD 1-2",
        std::string(m_WarpMenuIndex == 2 ? "! " : "  ") + "WORLD 1-3",
        "ENTER WARP",
        "F8 CLOSE"
    };

    const glm::vec2 origin(-112.0f, 88.0f);
    const float lineHeight = 34.0f;
    const std::size_t offset = 32;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        SetTextLine(offset + i, lines[i], { origin.x, origin.y - static_cast<float>(i) * lineHeight }, { 1.9f, 1.9f }, 2.0f, 92.0f);
        DrawTextLine(m_TextLines[offset + i]);
    }
}
