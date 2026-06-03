#ifndef __SJH_TEXTURE_H__
#define __SJH_TEXTURE_H__

#include "common/common.h"
#include "image.h"
#include "GL/gl3w.h"
#include <memory>

/**
 * @file texture.h
 * @brief 텍스처 RAII
 *
 * @details
 *  ### OpenGL 텍스처 사용 과정
 *  shader 프로그램이 바인딩 되었을때 사용하고자 하는 texture를 uniform 형태로 프로그램에 전달
 *  1. **객체 생성 + 바인딩** — @c glGenTextures / @c glBindTexture
 *  2. **wrapping / filtering 옵션 설정** — @c glTexParameteri
 *  3. **이미지 데이터를 GPU 메모리로 복사** — @c glTexImage2D
 *  4. **셰이더 바인딩 시점 sampler uniform 전달** — @c glUniform1i 로 텍스처 유닛 인덱스 전달
 */
namespace SJH
{
    CLASS_PTR(Texture)
    /**
     * @brief 단일 GL 2D 텍스처 객체의 RAII 래퍼 — 파일 로드 + GPU 업로드 일괄.
     * @details GL 핸들 단일 소유권 보장 — 복사 금지 / 이동만 허용
     *          (@c noexcept 이동으로 표준 컨테이너 재배치 친화).
     */
    class Texture
    {
    public:
        /**
         * @brief 빈 GL 텍스처를 지정 크기와 포맷으로 생성 — FBO 색상 어태치먼트 등 GPU-only 텍스처.
         * @param width   텍스처 너비 (픽셀).
         * @param height  텍스처 높이 (픽셀).
         * @param format  내부 포맷 (@c GL_RGBA, @c GL_RGB, @c GL_DEPTH24_STENCIL8 등).
         * @return 생성된 텍스처 (@c unique_ptr). 실패 시 @c nullptr.
         */
        static TextureUPtr Create(int width, int height, uint32_t format);
        /**
         * @brief 빈 GL 텍스처를 internalFormat/format/type 분리 지정으로 생성 — depth/packed 포맷용.
         * @param internalFormat GPU 저장 포맷 (@c GL_RGBA8, @c GL_DEPTH24_STENCIL8 등).
         * @param format         픽셀 데이터 채널 의미 (@c GL_RGBA, @c GL_DEPTH_STENCIL 등).
         * @param type           원소 타입 (@c GL_UNSIGNED_BYTE, @c GL_UNSIGNED_INT_24_8 등).
         * @return 생성된 텍스처 (@c unique_ptr). 실패 시 @c nullptr.
         * @note depth/packed 텍스처는 보간/mipmap 부적합 -> 본 overload 는 @c GL_NEAREST 필터 + @c GL_CLAMP_TO_EDGE wrap 으로 생성.
         */
        static TextureUPtr Create(int width, int height,
                                  uint32_t internalFormat, uint32_t format, uint32_t type);
        /**
         * @brief 디코드된 Image 로부터 GL 텍스처를 생성하고 GPU 에 업로드.
         * @param image  CPU 측 픽셀 데이터 컨테이너 (소유권 X — 호출 동안만 유효하면 됨).
         * @return 생성된 텍스처 (단일 소유 @c unique_ptr). 실패 시 @c nullptr 반환 가능.
         * @note 채널 수에 따라 자동 포맷 선택 — 1->@c GL_RED, 2->@c GL_RG, 3->@c GL_RGB, 그 외->@c GL_RGBA.
         *       @c internalformat 은 항상 @c GL_RGBA 로 고정 (학습용 단순화 — SRGB 등 확장 여지).
         */
        static TextureUPtr CreateTexture(const Image *image);

        /// @brief @c glDeleteTextures 로 GL 핸들 해제.
        ~Texture();

        Texture(const Texture &) = delete; ///< GL 핸들 단일 소유 — 복사 금지.
        Texture &operator=(const Texture &) = delete;
        Texture(Texture &&) noexcept; ///< @c noexcept 이동 — STL 컨테이너 재배치 안전.
        Texture &operator=(Texture &&) noexcept;

        int GetWidth() const { return mWidth; }
        int GetHeight() const { return mHeight; }
        uint32_t GetFormat() const { return mFormat; } ///< @brief GL 내부 포맷 (@c GL_RGBA 등).
        /// @brief GL 텍스처 핸들 — @c glBindTexture / @c glUniform1i 인자.
        GLuint GetTextureID() const { return mTextureID; }
        /// @brief @c GL_TEXTURE_2D 타깃에 본 텍스처를 바인딩.
        void Bind() const;
        /// @brief 축소/확대 필터 설정 (@c GL_TEXTURE_MIN_FILTER / @c GL_TEXTURE_MAG_FILTER).
        void SetFilter(GLuint minFilter, GLuint magFilter) const;
        /// @brief S/T 좌표 wrap 모드 설정 (@c GL_TEXTURE_WRAP_S / @c GL_TEXTURE_WRAP_T).
        void SetWrap(GLuint sWrap, GLuint tWrap) const;

        /// @brief 같은 GL 핸들을 유지하고 색상 스토리지를 새 크기로 재할당 (FBO 어태치먼트 리사이즈용).
        /// @details 핸들(@c GetTextureID) 불변 -> 이 텍스처를 sampler 로 참조하는 모든 consumer 가
        ///          재바인딩 없이 새 크기를 인식. 포맷은 기존 @c mFormat 유지.
        /// @param width  새 너비 (픽셀). @param height 새 높이 (픽셀).
        void Resize(int width, int height);

    private:
        Texture() = default;
        void CreateTexture();                         ///< @c glGenTextures + 기본 필터/wrap 설정.
        void SetTextureFromImage(const Image *image); ///< @c glTexImage2D 로 GPU 업로드.
        void SetTextureFormat(int width, int height, uint32_t format); ///< @c glTexImage2D 로 빈 GPU 메모리 할당.
        void SetTextureFormat(int width, int height,
                              uint32_t internalFormat, uint32_t format, uint32_t type); ///< 5-arg — internalFormat/format/type 분리.
        GLuint mTextureID = 0;     ///< GL 텍스처 핸들 — 0 은 invalid.
        int mWidth{0};             ///< 텍스처 너비 (픽셀). @ref Create / @ref SetTextureFromImage 가 설정.
        int mHeight{0};            ///< 텍스처 높이 (픽셀).
        uint32_t mFormat{GL_RGBA}; ///< GL 내부 포맷. @ref Create 가 설정; @ref CreateTexture 는 채널 수로 자동 선택.
        uint32_t mDataFormat{GL_RGBA};        ///< glTexImage2D 의 format 인자 (GL_DEPTH_STENCIL 등). Resize 재할당용.
        uint32_t mDataType{GL_UNSIGNED_BYTE}; ///< glTexImage2D 의 type 인자 (GL_UNSIGNED_INT_24_8 등). Resize 재할당용.
    };

}
#endif //__SJH_TEXTURE_H__
