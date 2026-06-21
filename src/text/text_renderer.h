/**
 * @file text_renderer.h
 * @brief 월드 공간 텍스트 렌더링 컴포넌트 - 글리프마다 child Actor(SpriteRenderer) 빌보드 발행.
 *
 * @details
 *  ### 책임
 *  - @c SetText 호출 시 문자열을 글자 단위로 분해해 owner Actor 의 child 로 추가.
 *  - 각 글리프 child 에 @c SpriteRenderer 를 부착하고 frame / tint 를 설정.
 *  - center 정렬 + 하단중앙 앵커(원점이 텍스트 하단 중앙) + 월드 X 배치 (spec sec.3).
 *  - @c SetColor / @c SetAlpha 로 기존 글리프 child 의 tint 를 일괄 갱신.
 *
 *  ### 비-책임
 *  - [X] Tween/IPlayable 구동 - @c SetAlpha / @c SetColor 만 노출, 구동은 Client(@c TweenPlayable 등) 책임.
 *  - [X] 글리프 child 소유 - owner Actor 가 children 소유 (@c RemoveChild 로 반환 후 파괴).
 *  - [X] 멀티라인 레이아웃 - 단일 행 전용 (줄바꿈 @c '\\n' 미지원).
 *
 * @note 2026-06-02 spec sec.3: @c SJH::Text World Text 모듈 일원.
 */
#ifndef __SJH_TEXT_TEXT_RENDERER_H__
#define __SJH_TEXT_TEXT_RENDERER_H__

#include "scene/actor.h"   // Component 베이스 + Actor
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace SJH::Text
{
    class BitmapFont;

    /**
     * @brief 문자열을 글리프 child Actor(SpriteRenderer 빌보드) 로 펼쳐
     *        월드 공간에 텍스트를 렌더링하는 @c Component.
     *
     * @details
     *  ### 레이아웃 규칙
     *  - 정렬: center (총 폭의 절반을 좌측 오프셋으로 사용).
     *  - 앵커: 하단 중앙 - @c Translate.y = charHeight * 0.5f (글자 중심이 1글자 높이의 절반 위).
     *  - 폰트 픽셀 단위를 월드 단위로 변환: worldPerPx = charHeight / cellH (px).
     *
     *  ### 사용 예
     *  @code
     *  auto* tr = actor->AddComponent<SJH::Text::TextRenderer>(font);
     *  tr->SetCharHeight(0.8f);   // SetText 전 설정
     *  tr->SetText("Hello");
     *  tr->SetAlpha(0.0f);        // fade in 은 TweenPlayable 등에서 구동
     *  @endcode
     *
     * @note @c OnExit 에서 @c mGlyphs 포인터 벡터만 초기화 - child 파괴는 owner Actor 의 소멸자 책임.
     */
    class TextRenderer : public SJH::Scene::Component
    {
      public:
        /**
         * @brief @p font 포인터를 주입받아 초기화.
         * @param font 사용할 @c BitmapFont (비소유 - ResourceRegistry 가 owner). nullptr 허용(IsValid 체크 후 렌더 스킵).
         */
        explicit TextRenderer(const BitmapFont* font) : mFont(font) {}

        /// @brief owner Actor 의 씬 편입 시 호출 - 텍스트 렌더러는 OnEnter 에서 별도 작업 없음.
        void OnEnter() override {}

        /// @brief owner Actor 의 씬 이탈 시 호출 - 글리프 포인터 벡터 초기화 (child 파괴는 owner 책임).
        void OnExit() override { mGlyphs.clear(); }

        /// @brief 매 프레임 틱 없음 - 정적 텍스트 (tint 변경은 @c SetColor / @c SetAlpha 로 직접 호출).
        void Update(float) override {}

        /**
         * @brief 표시할 문자열을 설정하고 글리프 child 를 재구성.
         * @details 기존 child 를 owner 에서 제거한 뒤 새 문자열로 Rebuild.
         *          @c SetCharHeight 는 이 메서드 *호출 전* 에 설정해야 반영.
         * @param s 표시할 ASCII 또는 Latin-1 문자열.
         */
        void SetText(const std::string& s);

        /**
         * @brief 모든 글리프 child 의 tint 를 @p rgba 로 일괄 설정.
         * @param rgba RGBA 색상 (각 채널 0~1).
         */
        void SetColor(const glm::vec4& rgba);

        /**
         * @brief 모든 글리프 child 의 알파를 @p a 로 설정 (fade 효과).
         * @param a 알파 값 (0=완전 투명, 1=불투명).
         */
        void SetAlpha(float a);

        /**
         * @brief 글리프 1자의 월드 단위 높이 설정. @c SetText 호출 *전* 에 설정해야 반영.
         * @details 픽셀->월드 비율 = @p worldH / @c BitmapFont::CellH().
         *          기본값 @c 0.5f (월드 단위).
         * @param worldH 1글자 높이 (world unit).
         */
        void SetCharHeight(float worldH) { mCharHeight = worldH; }

      private:
        /**
         * @brief 현재 @c mText 로 글리프 child 를 재구성.
         * @details owner 에서 기존 child 제거 -> center offset 계산 -> 각 문자별
         *          child Actor 추가 + @c SpriteRenderer 부착 + frame/tint 설정 -> @c mGlyphs 갱신.
         */
        void Rebuild();

        const BitmapFont* mFont = nullptr;                   ///< @brief 비소유 폰트 포인터 (ResourceRegistry 보유).
        std::string       mText;                             ///< @brief 현재 표시 중인 문자열.
        float             mCharHeight = 0.5f;                ///< @brief 글리프 1자의 월드 단위 높이 (기본 0.5).
        glm::vec4       mColor = glm::vec4(1, 1, 1, 1);  ///< @brief 전체 글리프 tint 색상 (RGBA, 기본 흰색).
        std::vector<SJH::Scene::Actor*> mGlyphs;             ///< @brief 글리프 child 포인터 벡터 (비소유 - owner children 소유).
    };

} // namespace SJH::Text

#endif // __SJH_TEXT_TEXT_RENDERER_H__
