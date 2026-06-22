/**
 * @file pass_iterator.h
 * @brief IPassable 들을 순서대로 실행하는 평탄 반복자 - before/GetPassResult 텍스처 체이닝 (D2/D8).
 *
 * @details
 *  Application 이 보유한 @c IPassable* 들을 코스 순서(@c Add 순서)로 @c Execute.
 *  각 Pass 의 @c BeforeIndex 가 가리키는 선행 Pass 의 @c GetPassResult() 를 @c before 로 주입 ->
 *  Pass 간 *텍스처 체이닝*(D2). 효과 재정렬/끼워넣기를 FBO 사전배선 없이 동적으로 표현하는 메커니즘.
 *
 *  비소유: Pass 수명은 Application(예: unique_ptr 멤버) 책임. 본 반복자는 raw 포인터만 보관.
 *
 * @note GL 격리: 이 헤더는 GL 헤더를 include 하지 않는다 (전방선언만).
 */
#ifndef __SJH_PASS_ITERATOR_H__
#define __SJH_PASS_ITERATOR_H__

#include <vector>

namespace SJH
{
	class IPassable;
	class DeviceContext;
	class RenderTarget;

	/**
	 * @brief IPassable 평탄 실행 반복자 - 코스 순서(Add) + before/GetPassResult 체이닝(D2/D8).
	 * @details main 은 mStages 더미 루프 대신 본 반복자 하나만 Execute (Application -> 단일 진입점).
	 */
	class PassIterator
	{
	  public:
		/// @brief Pass 등록(비소유). @return 등록 인덱스 - 다른 Pass 의 @c BeforeIndex 지정에 사용.
		int Add(IPassable *pass);

		/// @brief 등록 순서대로 각 Pass 실행.
		/// @details 각 Pass 는 자기 @c BeforeIndex 가 가리키는 선행 Pass 의 @c GetPassResult() 를
		///          @c before 로 받는다(범위밖/-1 이면 nullptr = scene raw). @c DebugPassIndex 까지만 실행 옵션.
		/// @param rec        GL facade (DeviceContext).
		/// @param backbuffer 최종 출력 backbuffer - 종단 Pass(present/ImGui)가 자체 보유하므로 현재 미사용.
		void Execute(DeviceContext &rec, RenderTarget &backbuffer);

		/// @brief Window resize 전파 - 등록된 각 Pass 의 @c OnResize 호출.
		void Resize(int w, int h);

		/// @brief 디버그 - 이 인덱스 Pass 까지만 실행하고 중단(-1=전체). PostFX 단계별 디버깅용.
		int DebugPassIndex = -1;

	  private:
		std::vector<IPassable *> mPasses; ///< 등록된 Pass(비소유). 수명은 Application.
	};
}

#endif // __SJH_PASS_ITERATOR_H__
