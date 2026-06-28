/**
 * @file render_enums.h
 * @brief 그래픽 API 중립 렌더 enum - 클라이언트/엔진이 raw @c GL_* 상수 대신 사용하는 래핑 값.
 *
 * @details
 *  ### 목적
 *  순수 OpenGL 공개 상수(@c GL_REPEAT / @c GL_LINEAR / @c GL_TRIANGLES 등)가 엔진 public
 *  API 시그니처와 클라이언트 호출부로 새어 나가는 것을 막는다. 이 헤더의 enum 은
 *  GL 헤더에 *전혀* 의존하지 않으므로(plain @c enum @c class), 장차 Vulkan/Metal 백엔드로
 *  교체하더라도 호출부는 그대로 두고 엔진 내부 @c ToGL* 매핑만 바꾸면 된다.
 *
 *  ### 변환 책임
 *  - 이 헤더는 *의미값만* 정의한다. 실제 @c GLenum 으로의 매핑은 GL 을 직접 호출하는
 *    각 엔진 모듈의 @c .cpp 내부(@c texture.cpp / @c mesh.cpp 등)에 격리된다.
 *
 *  ### 비-책임
 *  - [X] GL 로더(@c gl3w) 포함 - @c common 레이어와 동일하게 GL 비의존 유지.
 *  - [X] 모든 GL enum 망라 - 현재 엔진/클라이언트가 *실제로 쓰는* 값만 정의(YAGNI).
 */
#ifndef __SJH_RENDER_ENUMS_H__
#define __SJH_RENDER_ENUMS_H__

namespace SJH
{
    /// @brief 텍스처 좌표 wrap 모드. (GL: @c GL_REPEAT / @c GL_CLAMP_TO_EDGE)
    enum class WrapMode
    {
        Repeat,      ///< 좌표를 [0,1) 로 반복 - 타일링.
        ClampToEdge, ///< 가장자리 텍셀로 고정 - 경계 번짐 방지.
    };

    /// @brief 텍스처 축소/확대 필터. (GL: @c GL_NEAREST / @c GL_LINEAR / @c GL_LINEAR_MIPMAP_LINEAR)
    enum class FilterMode
    {
        Nearest,           ///< 최근접 - 픽셀아트.
        Linear,            ///< 선형 보간.
        LinearMipmapLinear, ///< 삼선형(mipmap 간 + 내부 선형) - 축소 필터 전용.
    };

    /// @brief 메시 프리미티브 토폴로지. (GL: @c GL_TRIANGLES)
    enum class PrimitiveTopology
    {
        Triangles, ///< 정점 3개당 삼각형 1개.
    };

} // namespace SJH
#endif // __SJH_RENDER_ENUMS_H__
