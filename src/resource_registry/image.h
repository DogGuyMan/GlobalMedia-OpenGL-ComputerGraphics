#ifndef __SJH_IMAGE_H__
#define __SJH_IMAGE_H__

/**
 * @file image.h
 * @brief stb_image 기반 디코딩 이미지의 CPU 측 RAII 래퍼.
 * @details GPU 업로드는 하지 않으며, 디코드된 픽셀 버퍼와 메타데이터(width / height / channelCount)만 보유.
 *          @c Texture 가 본 컨테이너를 입력으로 받아 GL 텍스처 객체로 업로드한다.
 */

#include <memory>
#include <string>
#include "common/common.h"
#include "GL/gl3w.h"
#include <vmath.h>
// 주의: @c stb_image.h 의 *함수 본문* 은 헤더에서 펼치지 않는다 (헤더에서 매크로 정의 시
//       Image.h 를 include 하는 모든 TU 에서 정의가 중복 -> linker duplicate symbols).
//       @c #define STB_IMAGE_IMPLEMENTATION 은 image.cpp 에서만 1회.

namespace SJH
{
    CLASS_PTR(Image)
    /**
     * @brief 디스크 이미지 파일을 디코드한 픽셀 데이터의 단일 소유권 컨테이너.
     * @details
     *  - @c stbi_load 로 디코드한 raw 픽셀(@c GLubyte*) + 메타데이터 보유
     *  - 소멸 시 @c stbi_image_free 자동 호출 (RAII)
     *  - 복사 금지, 이동만 허용 — 픽셀 버퍼 단일 소유 보장
     *  - GPU 업로드는 본 클래스 책임이 아님 (@c Texture::CreateTexture 로 위임)
     */
    class Image
    {

    public:
        /**
         * @brief 파일을 stb_image 로 디코드하여 Image 인스턴스를 생성.
         * @param filepath   이미지 파일 경로.
         * @param image_name 캐시 키로 사용할 논리 이름 (파일명과 별개).
         * @return 성공 시 @c ImageUPtr, 실패 시 @c nullptr.
         */
        static ImageUPtr Load(const std::string &image_name, const std::string &filepath);
        static ImageUPtr Create(const std::string &image_name, int width, int height, int channelCount = 4);

        /// @brief stb_image 가 할당한 픽셀 버퍼를 @c stbi_image_free 로 해제.
        ~Image();

        Image(const Image &) = delete;              ///< 픽셀 버퍼 단일 소유 — 복사 금지.
        Image &operator=(const Image &) = delete;
        Image(Image &&) noexcept;                   /// noexcept 이동 — STL 컨테이너 재배치 안전.
        Image &operator=(Image &&) noexcept;

        /// @brief 디코드된 raw 픽셀 포인터. 소유권 X — Image 수명 동안만 유효.
        const GLubyte *GetDataPtr() const { return mImageDataPtr; }

        /// @brief 캐시 키 / 디버그용 논리 이름.
        std::string GetImageName() const { return mImageName; }
        int GetWidth() const { return mWidth; }
        int GetHeight() const { return mHeight; }

        /// @brief 픽셀 채널 수 (1=R, 2=RG, 3=RGB, 4=RGBA).
        int GetChannelCount() const { return mChannelCount; }
        void SetCheckImage(int gridX, int gridY);
        void SetWhiteImage();
        void SetSingleColorImage(const vmath::vec4 &color);

    private:
        bool LoadWithStb(const std::string &filepath);
        bool Allocate(int width, int height, int channelCount);
        Image() = default;
        std::string mImageName;
        int mWidth{0};
        int mHeight{0};
        int mChannelCount{0};
        GLubyte *mImageDataPtr{nullptr};   ///< stb 가 할당 — 소멸자에서 stbi_image_free.
    };
};

#endif // __IMAGE_H__
