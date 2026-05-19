/**
 * @file texture.cpp
 * @brief Texture 정의 — Image 픽셀을 GL 텍스처 객체로 업로드하고 RAII 로 핸들 관리.
 */
#include "resource_registry.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace SJH
{

    TextureUPtr Texture::Create(int width, int height, uint32_t format)
    {
        auto texture = TextureUPtr(new Texture());
        texture->CreateTexture();
        texture->SetTextureFormat(width, height, format);
        texture->SetFilter(GL_LINEAR, GL_LINEAR);
        return std::move(texture);
    }

    /**
     * @note **Internal Format vs Format** — @c glTexImage2D 의 두 포맷 인자는 의미가 다르다.
     *  - @c internalformat : GPU 메모리에 텍스처를 *어떤 채널/비트 정밀도로 저장*할지 (저장 정밀도).
     *  - @c format         : CPU 측 입력 데이터의 *채널 순서* (@c GL_RGB / @c GL_RGBA / @c GL_BGR …).
     *  - @c type           : 입력 데이터의 원소 타입 (@c GL_UNSIGNED_BYTE, @c GL_FLOAT 등).
     */
    TextureUPtr Texture::CreateTexture(const Image *image)
    {
        auto texture = std::unique_ptr<Texture>(new Texture());
        texture->CreateTexture();
        texture->SetTextureFromImage(image);
        return std::move(texture);
    }

    Texture::~Texture()
    {
        if (mTextureID != 0)
            glDeleteTextures(1, &mTextureID);
    }

    Texture::Texture(Texture &&other) noexcept
        : mTextureID(other.mTextureID)
    {
        other.mTextureID = 0;
    }

    Texture &Texture::operator=(Texture &&other) noexcept
    {
        if (this != &other)
        {
            if (mTextureID != 0)
                glDeleteTextures(1, &mTextureID);
            mTextureID = other.mTextureID;
            other.mTextureID = 0;
        }
        return *this;
    }

    void Texture::Bind() const
    {
        glBindTexture(GL_TEXTURE_2D, mTextureID);
    }

    void Texture::SetFilter(GLuint minFilter, GLuint magFilter) const
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    }

    void Texture::SetWrap(GLuint sWrap, GLuint tWrap) const
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sWrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tWrap);
    }

    void Texture::CreateTexture()
    {
        glGenTextures(1, &mTextureID);
        // bind and set default filter and wrap option
        Bind();
        SetFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
        SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    void Texture::SetTextureFromImage(const Image *image)
    {
        GLenum format = GL_RGBA;
        switch (image->GetChannelCount())
        {
        default:
            break;
        case 1:
            format = GL_RED;
            break;
        case 2:
            format = GL_RG;
            break;
        case 3:
            format = GL_RGB;
            break;
        }

        mWidth = image->GetWidth();
        mHeight = image->GetHeight();
        mFormat = format;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     mWidth, mHeight, 0,
                     format, GL_UNSIGNED_BYTE,
                     image->GetDataPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void Texture::SetTextureFormat(int width, int height, uint32_t format)
    {
        mWidth = width;
        mHeight = height;
        mFormat = format;

        glTexImage2D(GL_TEXTURE_2D, 0, mFormat,
                     mWidth, mHeight, 0,
                     mFormat, GL_UNSIGNED_BYTE,
                     nullptr);
    }
}
