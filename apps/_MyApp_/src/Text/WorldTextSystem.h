/**
 * @file WorldTextSystem.h
 * @brief 월드 공간 텍스트 렌더링용 BMFont 1개를 보유하는 얇은 게임 시스템.
 *
 * @details
 *  ### 책임
 *  - 엔진 @c SJH::Text::BitmapFont 1개(minogram)를 로드/보유하고 접근점 제공.
 *  - @c VFXSystem / @c AudioSystem 과 평행한 위치의 게임 시스템 - @c Manager 가 값으로 보유.
 *
 *  ### 비-책임
 *  - [X] 실제 텍스트 그리기 - 엔진 @c SJH::Text::TextRenderer 가 담당. 본 시스템은 폰트만 공급.
 *  - [X] 폰트 자원 소유권 - 픽셀/텍스처는 @c SJH::ResourceRegistry 캐시에 위탁(@c Init 참조).
 *
 *  ### 정통 매핑
 *  - 게임의 폰트 매니저 - 전역 단일 폰트 핸들을 시스템들에 공급.
 *
 * @note 엔진 @c SJH::text 모듈(@c BitmapFont / @c TextRenderer)을 활용한다.
 */
#ifndef __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__
#define __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__

#include "text/bitmap_font.h"

namespace TopdownShooter::Text
{
    /**
     * @brief minogram BMFont 1개를 보유하는 얇은 시스템 (VFX/Audio 시스템 평행). @c Manager 보유.
     * @details 로드 성공 여부를 @c mLoaded 로 추적하고, 실패 시 @c GetFont 가 nullptr 을 반환한다.
     */
    class WorldTextSystem
    {
      public:
        /// @brief minogram BMFont 를 로드. @c Manager::Init 에서 1회 호출.
        /// @note 인자 없이 내부에서 ResourceRegistry::Get() 사용 — 호출자(Manager.cpp)가
        ///       resource_registry.h(gl3w.h)를 끌어오지 않게 해 Effekseer gl3.h 와의 GL 헤더 충돌 회피.
        void Init();

        /// @brief 로드된 BMFont 핸들 반환.
        /// @return 로드 성공 시 폰트 포인터, 아직 로드 안 됐거나 실패 시 nullptr.
        SJH::Text::BitmapFont* GetFont() { return mLoaded ? &mFont : nullptr; }

      private:
        SJH::Text::BitmapFont mFont;          ///< 보유 중인 minogram BMFont (atlas + Glyph 메타).
        bool mLoaded = false;                 ///< @c Init 성공 여부 - @c GetFont nullptr 게이트.
    };
}

#endif // __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__
