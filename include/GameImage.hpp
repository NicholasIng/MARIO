#pragma once

#include "pch.hpp"

#include "Core/Drawable.hpp"
#include "Core/Program.hpp"
#include "Core/UniformBuffer.hpp"
#include "Core/VertexArray.hpp"
#include "GameTexture.hpp"
#include "Util/AssetStore.hpp"

class GameImage : public Core::Drawable {
public:
    explicit GameImage(const std::string& filepath);

    glm::vec2 GetSize() const override { return m_Size; }

    void SetImage(const std::string& filepath);
    void Draw(const Core::Matrices& data) override;

private:
    void InitProgram();
    void InitVertexArray();

    static constexpr int UNIFORM_SURFACE_LOCATION = 0;

    static std::unique_ptr<Core::Program> s_Program;
    static std::unique_ptr<Core::VertexArray> s_VertexArray;
    static Util::AssetStore<std::shared_ptr<SDL_Surface>> s_Store;

    std::unique_ptr<Core::UniformBuffer<Core::Matrices>> m_UniformBuffer;
    std::unique_ptr<GameTexture> m_Texture;
    std::string m_Path;
    glm::vec2 m_Size = {0.0f, 0.0f};
};
