/**
 * @file pass_iterator.h
 * @brief IPassable 들을 순서대로 실행하는 평탄 반복자 - before/GetPassResult 텍스처 체이닝 (D2/D8).
 *
 * @details
 *  Application 이 보유한 @c IPassable* 들을 코스 순서(@c Add 순서)로 @c Execute.
 *  각 Pass 는 직전 *활성* Pass 의 @c GetPassResult()(lastResult)를 @c before 로 받는다 ->
 *  Pass 간 *동적 텍스처 체이닝*(design B). 비활성(@c Enabled=false) Pass 는 skip 되고
 *  체인은 자동 재연결(다음 활성 Pass 의 before = 마지막 활성 출력). 구 PassComponent 의
 *  정적 InputFB/OutputFB 배선 + bypass blit 을 대체 - FBO 사전배선/복사 없이 효과 on/off.
 *
 *  소유: PassIterator 가 Pass 를 @c unique_ptr 로 *소유* 하고 매 프레임 수행한다. 외부(Application)는
 *  Pass 포인터를 캐싱하지 말고, 필요 시 @c Find(key) 로 조회해 접근한다(키 = @c IPassable::GetPassKey).
 *
 * @note GL 격리: 이 헤더는 GL 헤더를 include 하지 않는다 (전방선언만).
 */
#ifndef __SJH_PASS_ITERATOR_H__
#define __SJH_PASS_ITERATOR_H__

#include <memory>
#include <string>
#include <vector>

namespace SJH
{
	class IPassable;
	class DeviceContext;
	class RenderTarget;

	/**
	 * @brief IPassable 평탄 실행 반복자 - Pass 소유 + 코스 순서(Add) + lastResult 동적 체이닝 + Enabled skip(design B).
	 * @details Application 은 Pass 별 멤버 포인터를 두지 않고 본 반복자 하나만 보유 - 등록(Add)/조회(Find)/수행(Execute).
	 */
	class PassIterator
	{
	  public:
		/// @brief Pass 소유권 이양 등록(PassIterator 가 unique_ptr 로 소유). @return 등록 인덱스.
		int Add(std::unique_ptr<IPassable> pass);

		/// @brief @c GetPassKey() 가 @p key 와 일치하는 첫 Pass 조회(비소유 반환, 즉시 사용용 - 캐싱 금지). 없으면 nullptr.
		/// @details 외부(Application)가 특정 Pass 에 접근(예: @c Enabled 토글)할 단일 경로. 소유/수명은 PassIterator.
		IPassable *Find(const std::string &key) const;

		/// @brief 등록된 모든 Pass 의 키(@c GetPassKey) 목록 - 코스 순서. 디버그 UI 열거용(PassDebugLayer).
		/// @details 반환 후 @c Find(key) 로 개별 Pass 접근(예: Enabled 토글). 키만 복사 반환(포인터 비노출).
		std::vector<std::string> Keys() const;

		/// @brief 등록 순서대로 각 *활성* Pass 실행.
		/// @details 각 활성 Pass 는 직전 활성 Pass 의 @c GetPassResult()(lastResult)를 @c before 로 받는다
		///          (첫 활성 Pass 는 nullptr = scene raw). @c Enabled=false Pass 는 skip - 체인 자동 재연결.
		///          @c GetPassResult()==nullptr 인 Pass(Particle/ImGui)는 lastResult 를 통과시킨다. @c DebugPassIndex 까지만 실행 옵션.
		/// @param rec        GL facade (DeviceContext).
		/// @param backbuffer 최종 출력 backbuffer - 종단 Pass(present/ImGui)가 자체 보유하므로 현재 미사용.
		void Execute(DeviceContext &rec, RenderTarget &backbuffer);

		/// @brief Window resize 전파 - 등록된 각 Pass 의 @c OnResize 호출.
		void Resize(int w, int h);

		/// @brief 등록된 모든 Pass 해제(소유 unique_ptr 파괴). shutdown 시 GL 컨텍스트가 살아있는 동안 호출.
		/// @details Pass 가 보유한 GL 자원(예: WorldPass 의 LightUboUploader UBO)을 컨텍스트 파괴 전에 정리하기 위함.
		void Clear();

		/// @brief 디버그 - 이 인덱스 Pass 까지만 실행하고 중단(-1=전체). PostFX 단계별 디버깅용.
		int DebugPassIndex = -1;

	  private:
		std::vector<std::unique_ptr<SJH::IPassable>> mPasses; ///< 등록된 Pass - PassIterator 소유(unique_ptr).
	};
}

#endif // __SJH_PASS_ITERATOR_H__
