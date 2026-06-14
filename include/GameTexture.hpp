#pragma once

#include "pch.hpp"

class GameTexture {
public:
    GameTexture(GLint format, int width, int height, const void* data);
    GameTexture(const GameTexture&) = delete;
    GameTexture(GameTexture&& texture) noexcept;

    ~GameTexture();

    GameTexture& operator=(const GameTexture&) = delete;
    GameTexture& operator=(GameTexture&& other) noexcept;

    GLuint GetTextureId() const { return m_TextureId; }

    void Bind(int slot) const;
    void Unbind() const;

    void UpdateData(GLint format, int width, int height, const void* data);

private:
    GLuint m_TextureId = 0;
};
