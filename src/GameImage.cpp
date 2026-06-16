#include "GameImage.hpp"

#include <filesystem>

#include "AssetPaths.hpp"
#include "Core/TextureUtils.hpp"
#include "Util/Logger.hpp"
#include "Util/MissingTexture.hpp"

namespace {
std::shared_ptr<SDL_Surface> LoadSurface(const std::string& filepath) {
    auto surface = std::shared_ptr<SDL_Surface>(IMG_Load(filepath.c_str()),
                                                SDL_FreeSurface);

    if (surface == nullptr) {
        surface = {GetMissingTextureSDLSurface(), SDL_FreeSurface};
        LOG_ERROR("Failed to load image: '{}'", filepath);
        LOG_ERROR("{}", IMG_GetError());
    }

    return surface;
}

std::filesystem::path ShaderPath(const char* filename) {
    return AssetPaths::ResourceRoot().parent_path() / "PTSD" / "assets" /
           "shaders" / filename;
}
} // namespace

GameImage::GameImage(const std::string& filepath)
    : m_Path(filepath) {
    if (s_Program == nullptr) {
        InitProgram();
    }
    if (s_VertexArray == nullptr) {
        InitVertexArray();
    }

    m_UniformBuffer = std::make_unique<Core::UniformBuffer<Core::Matrices>>(
        *s_Program, "Matrices", 0);

    auto surface = s_Store.Get(filepath);

    m_Texture = std::make_unique<GameTexture>(
        Core::SdlFormatToGlFormat(surface->format->format), surface->w,
        surface->h, surface->pixels);
    m_Size = {static_cast<float>(surface->w), static_cast<float>(surface->h)};
}

void GameImage::SetImage(const std::string& filepath) {
    auto surface = s_Store.Get(filepath);

    m_Texture->UpdateData(Core::SdlFormatToGlFormat(surface->format->format),
                          surface->w, surface->h, surface->pixels);
    m_Size = {static_cast<float>(surface->w), static_cast<float>(surface->h)};
    m_Path = filepath;
}

void GameImage::Draw(const Core::Matrices& data) {
    m_UniformBuffer->SetData(0, data);

    m_Texture->Bind(UNIFORM_SURFACE_LOCATION);
    s_Program->Bind();
    s_Program->Validate();

    s_VertexArray->Bind();
    s_VertexArray->DrawTriangles();
}

void GameImage::InitProgram() {
    s_Program = std::make_unique<Core::Program>(
        ShaderPath("Base.vert").string(), ShaderPath("Base.frag").string());
    s_Program->Bind();

    const GLint location = glGetUniformLocation(s_Program->GetId(), "surface");
    glUniform1i(location, UNIFORM_SURFACE_LOCATION);
}

void GameImage::InitVertexArray() {
    s_VertexArray = std::make_unique<Core::VertexArray>();

    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            -0.5F, 0.5F,
            -0.5F, -0.5F,
            0.5F, -0.5F,
            0.5F, 0.5F,
        },
        2));

    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            0.0F, 0.0F,
            0.0F, 1.0F,
            1.0F, 1.0F,
            1.0F, 0.0F,
        },
        2));

    s_VertexArray->SetIndexBuffer(
        std::make_unique<Core::IndexBuffer>(std::vector<unsigned int>{
            0, 1, 2,
            0, 2, 3,
        }));
}

std::unique_ptr<Core::Program> GameImage::s_Program = nullptr;
std::unique_ptr<Core::VertexArray> GameImage::s_VertexArray = nullptr;
Util::AssetStore<std::shared_ptr<SDL_Surface>> GameImage::s_Store(LoadSurface);
