#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

namespace Util {
class GameObject;
}

class App;
class GameImage;

class DebugManager {
public:
    enum class Flag : std::uint32_t {
        Overlay = 1u << 0,
        Hitboxes = 1u << 1,
        GodMode = 1u << 2,
        FreeCamera = 1u << 3,
        FlyMode = 1u << 4,
        WarpMenu = 1u << 5,
        Noclip = 1u << 6
    };

    DebugManager();

    void HandleHotkeys(App& app, float dt);
    void Render(App& app);

    bool IsOverlayEnabled() const { return HasFlag(Flag::Overlay); }
    bool AreHitboxesEnabled() const { return HasFlag(Flag::Hitboxes); }
    bool IsGodModeEnabled() const { return HasFlag(Flag::GodMode); }
    bool IsFreeCameraEnabled() const { return HasFlag(Flag::FreeCamera); }
    bool IsFlyModeEnabled() const { return HasFlag(Flag::FlyMode); }
    bool IsWarpMenuOpen() const { return HasFlag(Flag::WarpMenu); }
    bool IsNoclipEnabled() const { return HasFlag(Flag::Noclip); }

private:
    struct TextLine {
        std::string value;
        std::vector<std::shared_ptr<Util::GameObject>> glyphs;
    };

    struct BoxPrimitive {
        std::shared_ptr<Util::GameObject> top;
        std::shared_ptr<Util::GameObject> bottom;
        std::shared_ptr<Util::GameObject> left;
        std::shared_ptr<Util::GameObject> right;
    };

    std::uint32_t m_Flags = 0u;
    int m_WarpMenuIndex = 0;
    float m_LastFps = 0.0f;
    float m_FreeCameraSpeed = 720.0f;
    std::shared_ptr<GameImage> m_DebugPixel;
    std::vector<TextLine> m_TextLines;
    std::vector<BoxPrimitive> m_BoxPool;

    bool HasFlag(Flag flag) const;
    void SetFlag(Flag flag, bool enabled);
    void ToggleFlag(Flag flag);

    void SyncMarioDebugFlags(App& app) const;
    void UpdateFreeCamera(App& app, float dt);
    void HandleWarpMenuInput(App& app);
    void WarpToSelectedLevel(App& app);
    void SpawnGoombaAtMario(App& app);
    void SpawnMushroomNearMario(App& app);

    void DrawOverlay(App& app);
    void DrawWarpMenu(App& app);
    void DrawHitboxes(App& app);

    void SetTextLine(std::size_t index,
                     const std::string& text,
                     const glm::vec2& position,
                     const glm::vec2& scale,
                     float spacing,
                     float zIndex);
    void DrawTextLine(const TextLine& line) const;
    std::string FontPathForChar(char character) const;

    BoxPrimitive& AcquireBox(std::size_t index);
    void DrawOutlineBox(std::size_t& boxIndex,
                        const glm::vec2& center,
                        const glm::vec2& halfExtents,
                        float viewX,
                        float viewY,
                        float zIndex);
};
