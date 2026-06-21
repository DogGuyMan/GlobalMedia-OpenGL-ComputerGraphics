/**
 * @file image.h
 * @brief stb_image 기반 디코드 이미지의 CPU 측 RAII 컨테이너.
 *
 * @details
 *  ### 책임
 *  - @c stbi_load 로 디코드한 raw 픽셀 버퍼(@c GLubyte*) + 메타데이터(width/height/channelCount) 보유.
 *  - 소멸 시 @c stbi_image_free 자동 호출 (RAII).
 *  - 절차적 이미지 생성 - @c Create / @c SetCheckImage / @c SetWhiteImage / @c SetSingleColorImage.
 *
 *  ### 비-책임
 *  - [X] GPU 업로드 - @c Texture::CreateTexture 로 위임.
 *  - [X] @c stb_image 함수 본문 노출 - @c STB_IMAGE_IMPLEMENTATION 은 @c image.cpp 단 한 곳.
 *    (헤더에서 정의 시 include 하는 모든 TU 에 중복 -> 링커 duplicate symbol.)
 *
 * @note @c stbi_* 직접 호출 금지 - @c SJH::Image::Load / @c SJH::Image::Create 위임.
 */
#ifndef __SJH_IMAGE_H__
#define __SJH_IMAGE_H__

#include <memory>
#include <string>
#include "common/common.h"
#include "GL/gl3w.h"
#include <glm/glm.hpp>

namespace SJH
{
    CLASS_PTR(Image)
    /**
     * @brief 디스크/절차 이미지를 디코드한 픽셀 데이터의 단일 소유권 RAII 컨테이너.
     * @details
     *  - @c stbi_load 로 디코드한 raw 픽셀(@c GLubyte*) + 메타데이터(width/height/channelCount) 보유.
     *  - 소멸 시 @c stbi_image_free 자동 호출 (RAII).
     *  - 복사 금지, @c noexcept 이동만 허용 - 픽셀 버퍼 단일 소유 보장.
     *  - GPU 업로드는 본 클래스 책임이 아님 - @c Texture::CreateTexture 로 위임.
     */
    class Image
    {

    public:
        /**
         * @brief 파일을 @c stb_image 로 디코드하여 Image 인스턴스를 반환.
         * @param image_name 캐시 키로 사용할 논리 이름 (파일명과 별개).
         * @param filepath   이미지 파일 경로.
         * @return 성공 시 @c ImageUPtr, 실패(@c stbi_load 오류) 시 @c nullptr.
         */
        static ImageUPtr Load(const std::string &image_name, const std::string &filepath);

        /**
         * @brief 지정 크기의 빈 픽셀 버퍼를 @c malloc 으로 할당하여 Image 인스턴스를 반환.
         * @details 절차적 이미지(@c SetCheckImage / @c SetWhiteImage / @c SetSingleColorImage)의
         *          기반 버퍼 생성용. GPU 업로드는 @c Texture::CreateTexture 를 통해 별도로 수행.
         * @param image_name  논리 이름.
         * @param width       픽셀 너비.
         * @param height      픽셀 높이.
         * @param channelCount 채널 수 (기본 4 = RGBA).
         * @return 성공 시 @c ImageUPtr, 메모리 할당 실패 시 @c nullptr.
         */
        static ImageUPtr Create(const std::string &image_name, int width, int height, int channelCount = 4);

        /// @brief @c stb_image 가 할당한 픽셀 버퍼를 @c stbi_image_free 로 해제.
        ~Image();

        Image(const Image &) = delete;              ///< 픽셀 버퍼 단일 소유 - 복사 금지.
        Image &operator=(const Image &) = delete;
        Image(Image &&) noexcept;                   ///< @c noexcept 이동 - STL 컨테이너 재배치 안전.
        Image &operator=(Image &&) noexcept;

        /// @brief 디코드된 raw 픽셀 포인터. 소유권 X - Image 수명 동안만 유효.
        const GLubyte *GetDataPtr() const { return mImageDataPtr; }

        /// @brief 캐시 키 / 디버그용 논리 이름.
        std::string GetImageName() const { return mImageName; }
        /// @brief 이미지 너비 (픽셀).
        int GetWidth() const { return mWidth; }
        /// @brief 이미지 높이 (픽셀).
        int GetHeight() const { return mHeight; }
        /// @brief 픽셀 채널 수 (1=R, 2=RG, 3=RGB, 4=RGBA).
        int GetChannelCount() const { return mChannelCount; }

        /// @brief 체커보드 패턴으로 픽셀 버퍼를 채움 (테스트/폴백 텍스처용).
        /// @param gridX X 방향 격자 셀 너비 (픽셀).
        /// @param gridY Y 방향 격자 셀 높이 (픽셀).
        void SetCheckImage(int gridX, int gridY);
        /// @brief 전체 픽셀 버퍼를 흰색(0xFF)으로 채움.
        void SetWhiteImage();
        /// @brief 전체 픽셀 버퍼를 단색 @p color (RGBA, 0~1) 로 채움.
        /// @param color RGBA 색상 (각 채널 0.0f ~ 1.0f).
        void SetSingleColorImage(const glm::vec4 &color);

    private:
        bool LoadWithStb(const std::string &filepath);
        bool Allocate(int width, int height, int channelCount);
        Image() = default;
        std::string mImageName;
        int mWidth{0};
        int mHeight{0};
        int mChannelCount{0};
        GLubyte *mImageDataPtr{nullptr};   ///< @c stb_image 또는 @c malloc 이 할당 - 소멸자에서 @c stbi_image_free.
    };
};

#endif // __SJH_IMAGE_H__
