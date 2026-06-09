/**
 * @file texture.h
 * @brief OpenGL 2D 텍스처 객체의 RAII 래퍼 - 이미지 로드/GPU 업로드/핸들 관리 일괄.
 *
 * @details
 *  ### 책임
 *  - GL 텍스처 핸들(@c GLuint) 단일 소유 + 소멸 시 @c glDeleteTextures 자동 호출.
 *  - @c Image(CPU 픽셀) -> GPU 텍스처 업로드(@c CreateTexture(Image*)).
 *  - 빈 GPU 텍스처 생성 - FBO 색상/depth 어태치먼트용(@c Create 오버로드 2종).
 *  - 핸들 불변 리사이즈(@c Resize) - FBO 어태치먼트가 재바인딩 없이 새 크기 인식.
 *
 *  ### OpenGL 텍스처 생성 절차
 *  1. **객체 생성 + 바인딩** - @c glGenTextures / @c glBindTexture
 *  2. **wrapping / filtering 설정** - @c glTexParameteri
 *  3. **이미지 데이터 GPU 업로드** - @c glTexImage2D
 *  4. **셰이더 바인딩 시 sampler uniform 전달** - @c glUniform1i 로 텍스처 유닛 인덱스 전달
 *
 *  ### 비-책임
 *  - [X] CPU 픽셀 데이터 보관 - @c Image 가 소유, 본 클래스는 GPU 핸들만 보유.
 *  - [X] 멀티샘플 / 3D / Cube 텍스처 - @c GL_TEXTURE_2D 전용.
 *
 * @note 픽셀아트 NEAREST 필터는 생성 후 @c SetFilter(GL_NEAREST, GL_NEAREST) 추가 호출.
 *       @c UniformAtlas::LoadFromPNG 내부에서 자동 적용.
 */
#ifndef __SJH_TEXTURE_H__
#define __SJH_TEXTURE_H__

#include "common/common.h"
#include "image.h"
#include "GL/gl3w.h"
#include <memory>
namespace SJH
{
    CLASS_PTR(Texture)
    /**
     * @brief 단일 @c GL_TEXTURE_2D 객체의 RAII 래퍼.
     * @details
     *  GL 핸들(@c GLuint) 단일 소유권 보장 - 복사 금지 / @c noexcept 이동만 허용
     *  (표준 컨테이너 재배치 친화). @c ResourceRegistry 가 @c unique_ptr 로 보유하며,
     *  매니저 소멸 시 @c glDeleteTextures 자동 호출.
     */
    class Texture
    {
    public:
        /**
         * @brief 빈 GL 텍스처를 지정 크기와 단일 포맷으로 생성 - FBO 색상 어태치먼트 등 GPU-only 용.
         * @details @c internalFormat = @p format, @c type = @c GL_UNSIGNED_BYTE 로 위임.
         *          기본 필터 @c GL_LINEAR / wrap @c GL_CLAMP_TO_EDGE.
         * @param width   텍스처 너비 (픽셀).
         * @param height  텍스처 높이 (픽셀).
         * @param format  내부 포맷 (@c GL_RGBA, @c GL_RGB 등).
         * @return 생성된 텍스처 (@c TextureUPtr). 실패 시 @c nullptr.
         */
        static TextureUPtr Create(int width, int height, uint32_t format);

        /**
         * @brief 빈 GL 텍스처를 @c internalFormat / @c format / @c type 분리 지정으로 생성 - depth/packed 포맷용.
         * @details depth/packed 텍스처는 mipmap 생성 및 선형 보간 부적합 -
         *          본 오버로드는 @c GL_NEAREST 필터 + @c GL_CLAMP_TO_EDGE wrap 으로 강제 설정.
         * @param width          텍스처 너비 (픽셀).
         * @param height         텍스처 높이 (픽셀).
         * @param internalFormat GPU 저장 포맷 (@c GL_RGBA8, @c GL_DEPTH24_STENCIL8 등).
         * @param format         픽셀 데이터 채널 의미 (@c GL_RGBA, @c GL_DEPTH_STENCIL 등).
         * @param type           원소 타입 (@c GL_UNSIGNED_BYTE, @c GL_UNSIGNED_INT_24_8 등).
         * @return 생성된 텍스처 (@c TextureUPtr). 실패 시 @c nullptr.
         */
        static TextureUPtr Create(int width, int height,
                                  uint32_t internalFormat, uint32_t format, uint32_t type);

        /**
         * @brief 디코드된 @c Image 로부터 GL 텍스처를 생성하고 GPU 에 업로드.
         * @details 채널 수에 따라 @c format 자동 선택 - 1->@c GL_RED, 2->@c GL_RG, 3->@c GL_RGB, 그 외->@c GL_RGBA.
         *          @c internalFormat 은 항상 @c GL_RGBA 고정 (학습용 단순화 - SRGB 확장 여지).
         *          업로드 후 @c glGenerateMipmap 자동 호출.
         * @param image CPU 측 픽셀 데이터 컨테이너 (소유권 X - 호출 스택 동안만 유효하면 됨).
         * @return 생성된 텍스처 (@c TextureUPtr). 실패 시 @c nullptr.
         */
        static TextureUPtr CreateTexture(const Image *image);

        /// @brief @c glDeleteTextures 로 GL 핸들 해제.
        ~Texture();

        Texture(const Texture &) = delete;              ///< GL 핸들 단일 소유 - 복사 금지.
        Texture &operator=(const Texture &) = delete;
        Texture(Texture &&) noexcept;                   ///< @c noexcept 이동 - STL 컨테이너 재배치 안전.
        Texture &operator=(Texture &&) noexcept;

        /// @brief 텍스처 너비 (픽셀).
        int GetWidth() const { return mWidth; }
        /// @brief 텍스처 높이 (픽셀).
        int GetHeight() const { return mHeight; }
        /// @brief GL 내부 포맷 (@c GL_RGBA 등).
        uint32_t GetFormat() const { return mFormat; }
        /// @brief GL 텍스처 핸들 - @c glBindTexture / @c glUniform1i 인자. 0 은 invalid.
        GLuint GetTextureID() const { return mTextureID; }
        /// @brief @c GL_TEXTURE_2D 타깃에 본 텍스처를 바인딩.
        void Bind() const;
        /// @brief 축소/확대 필터 설정 (@c GL_TEXTURE_MIN_FILTER / @c GL_TEXTURE_MAG_FILTER).
        /// @param minFilter 축소 필터 (@c GL_LINEAR, @c GL_NEAREST, @c GL_LINEAR_MIPMAP_LINEAR 등).
        /// @param magFilter 확대 필터 (@c GL_LINEAR, @c GL_NEAREST).
        void SetFilter(GLuint minFilter, GLuint magFilter) const;
        /// @brief S/T 좌표 wrap 모드 설정 (@c GL_TEXTURE_WRAP_S / @c GL_TEXTURE_WRAP_T).
        /// @param sWrap S(U) 축 wrap (@c GL_CLAMP_TO_EDGE, @c GL_REPEAT 등).
        /// @param tWrap T(V) 축 wrap.
        void SetWrap(GLuint sWrap, GLuint tWrap) const;

        /**
         * @brief 같은 GL 핸들을 유지한 채 텍스처 스토리지를 새 크기로 재할당 - FBO 어태치먼트 리사이즈용.
         * @details @c GetTextureID 불변 -> 이 텍스처를 sampler 로 참조하는 모든 consumer 가
         *          재바인딩 없이 새 크기를 인식. 포맷(@c mFormat/@c mDataFormat/@c mDataType) 은 유지.
         * @param width  새 너비 (픽셀).
         * @param height 새 높이 (픽셀).
         */
        void Resize(int width, int height);

    private:
        Texture() = default;
        void CreateTexture();                         ///< @c glGenTextures + 기본 필터(@c GL_LINEAR_MIPMAP_LINEAR) / wrap(@c GL_CLAMP_TO_EDGE) 설정.
        void SetTextureFromImage(const Image *image); ///< @c glTexImage2D 로 CPU 픽셀 GPU 업로드 + @c glGenerateMipmap.
        void SetTextureFormat(int width, int height, uint32_t format); ///< 3-arg - @c internalFormat=@p format, @c type=@c GL_UNSIGNED_BYTE 로 5-arg 위임.
        void SetTextureFormat(int width, int height,
                              uint32_t internalFormat, uint32_t format, uint32_t type); ///< 5-arg - @c glTexImage2D 빈 스토리지 할당 + 멤버 설정.
        GLuint mTextureID = 0;                ///< GL 텍스처 핸들 - 0 은 invalid.
        int mWidth{0};                        ///< 텍스처 너비 (픽셀). @ref Create / @ref SetTextureFromImage 가 설정.
        int mHeight{0};                       ///< 텍스처 높이 (픽셀).
        uint32_t mFormat{GL_RGBA};            ///< GL 내부(저장) 포맷. @ref Create 가 설정; @ref CreateTexture 는 채널 수 자동 선택.
        uint32_t mDataFormat{GL_RGBA};        ///< @c glTexImage2D 의 @c format 인자 (@c GL_DEPTH_STENCIL 등). @c Resize 재할당용.
        uint32_t mDataType{GL_UNSIGNED_BYTE}; ///< @c glTexImage2D 의 @c type 인자 (@c GL_UNSIGNED_INT_24_8 등). @c Resize 재할당용.
    };

} // namespace SJH
#endif // __SJH_TEXTURE_H__
