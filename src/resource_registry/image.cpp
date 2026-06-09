/**
 * @file image.cpp
 * @brief @c Image 메서드 정의 - @c stb_image implementation 을 이 TU 에서만 펼친다.
 *
 * @details
 *  ### STB_IMAGE_IMPLEMENTATION 단일 소유
 *  @c STB_IMAGE_IMPLEMENTATION 매크로는 프로젝트 전체에서 정확히 이 파일 한 곳에서만 정의.
 *  단일 헤더 라이브러리 규칙 - 다른 @c .cpp 에서 중복 정의 시 링커 duplicate symbol 에러.
 *  @c stbi_* 직접 호출 금지 - @c SJH::Image::Load / @c SJH::Image::Create 경유.
 *
 * @note @c stbi_set_flip_vertically_on_load(true) 는 @c LoadWithStb 에서 매 호출마다 설정 -
 *       OpenGL UV 좌표(Y=0 하단) 와 PNG(Y=0 상단) 의 방향 불일치 보정.
 */
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace SJH
{
    ImageUPtr Image::Create(const std::string &image_name, int width, int height, int channelCount)
    {
        auto image = ImageUPtr(new Image());
        if (!image->Allocate(width, height, channelCount))
            return nullptr;
        image->mImageName = image_name;
        return std::move(image);
    }

    ImageUPtr Image::Load(const std::string &image_name, const std::string &filepath)
    {
        auto image = std::unique_ptr<Image>(new Image());
        if (!image->LoadWithStb(filepath))
            return nullptr;
        image->mImageName = image_name;
        return std::move(image);
    }

    Image::~Image()
    {
        if (mImageDataPtr != 0)
            stbi_image_free(mImageDataPtr);
    }

    Image::Image(Image &&other) noexcept
        : mImageName(other.mImageName),
          mWidth(other.mWidth), mHeight(other.mHeight), mChannelCount(other.mChannelCount),
          mImageDataPtr(other.mImageDataPtr)
    {
        other.mImageDataPtr = nullptr;
    }

    Image &Image::operator=(Image &&other) noexcept
    {
        if (this != &other)
        {
            if (mImageDataPtr != 0)
                stbi_image_free(mImageDataPtr);
            mImageDataPtr = other.mImageDataPtr;
            other.mImageDataPtr = 0;
            mImageName = other.mImageName;
            mWidth = other.mWidth;
            mHeight = other.mHeight;
            mChannelCount = other.mChannelCount;
        }
        return *this;
    }

    bool Image::LoadWithStb(const std::string &filepath)
    {
        stbi_set_flip_vertically_on_load(true);

        mImageDataPtr = stbi_load(filepath.c_str(), &mWidth, &mHeight, &mChannelCount, 0);
        if (mImageDataPtr == 0)
        {
            spdlog::error("이미지 로딩 실패: {}", filepath);
            return false;
        }

        return true;
    }

    bool Image::Allocate(int width, int height, int channelCount)
    {
        mWidth = width;
        mHeight = height;
        mChannelCount = channelCount;
        mImageDataPtr = (GLubyte *)malloc(mWidth * mHeight * mChannelCount);
        return mImageDataPtr ? true : false;
    }

    // 이 내용은 나중에 자유함수로 빼자.
    void Image::SetCheckImage(int gridX, int gridY)
    {
        for (int j = 0; j < mHeight; j++)
        {
            for (int i = 0; i < mWidth; i++)
            {
                int pos = (j * mWidth + i) * mChannelCount;
                bool even = ((i / gridX) + (j / gridY)) % 2 == 0;
                uint8_t value = even ? 255 : 0;
                for (int k = 0; k < mChannelCount; k++)
                    mImageDataPtr[pos + k] = value;
                if (mChannelCount > 3)
                    mImageDataPtr[3] = 255;
            }
        }
    }
    void Image::SetWhiteImage()
    {
        std::memset(mImageDataPtr, 255, mWidth * mHeight * mChannelCount);
    }

    void Image::SetSingleColorImage(const vmath::vec4 &color)
    {
        auto v = color * 255.0f;
        uint8_t rgba[4] = {
            (uint8_t)std::clamp(v[0], 0.0f, 255.0f),
            (uint8_t)std::clamp(v[1], 0.0f, 255.0f),
            (uint8_t)std::clamp(v[2], 0.0f, 255.0f),
            (uint8_t)std::clamp(v[3], 0.0f, 255.0f),
        };
        for (int i = 0; i < mWidth * mHeight; i++)
            std::memcpy(mImageDataPtr + 4 * i, rgba, 4);
    }
}
