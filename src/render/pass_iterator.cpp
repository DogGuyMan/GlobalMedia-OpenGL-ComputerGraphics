/**
 * @file pass_iterator.cpp
 * @brief PassIterator::Execute - before/GetPassResult 체이닝 + bounds check + DebugPassIndex.
 */
#include "render/pass_iterator.h"
#include "render/render_passable/render_passable.h" // IPassable (Draw/GetPassResult/BeforeIndex/OnResize)

namespace SJH
{
	int PassIterator::Add(std::unique_ptr<IPassable> pass)
	{
		mPasses.push_back(std::move(pass)); // 소유권 이양 - PassIterator 가 수명 책임.
		return static_cast<int>(mPasses.size()) - 1;
	}

	IPassable *PassIterator::Find(const std::string &key) const
	{
		// GetPassKey() 일치 첫 Pass 반환(비소유, 즉시 사용용). 외부는 캐싱 말고 매번 조회.
		for (const auto &p : mPasses)
			if (p && p->GetPassKey() == key)
				return p.get();
		return nullptr;
	}

	void PassIterator::Resize(int w, int h)
	{
		for (const auto &p : mPasses)
			if (p)
				p->OnResize(w, h);
	}

	void PassIterator::Clear()
	{
		mPasses.clear(); // 소유 unique_ptr 파괴 - 호출 시점(shutdown)에 GL 컨텍스트가 살아있어야 한다.
	}

	std::vector<std::string> PassIterator::Keys() const
	{
		std::vector<std::string> keys;
		keys.reserve(mPasses.size());
		for (const auto &p : mPasses)
			if (p)
				keys.push_back(p->GetPassKey());
		return keys;
	}

	void PassIterator::Execute(DeviceContext &rec, RenderTarget & /*backbuffer*/)
	{
		// 동적 체이닝(Enable 메커니즘, design B) - PassComponent 의 정적 InputFB/OutputFB 배선 대체.
		//  before = 직전 *활성* Pass 의 GetPassResult (lastResult). 비활성 Pass 는 skip -> 체인 자동 재연결.
		const Texture *lastResult = nullptr; // 마지막 활성 Pass 의 출력(체인 입력). nullptr=scene raw.
		const int      passCnt          = static_cast<int>(mPasses.size());
		for (int i = 0; i < passCnt; ++i)
		{
			IPassable *p = mPasses[i].get(); // unique_ptr -> raw (소유는 mPasses 유지).
			if (!p || !p->Enabled)
				continue; // [D-1] 비활성 skip - bypass blit 안 함(체인은 lastResult 가 자동 재연결).

			p->Draw(rec, lastResult); // [D-2] before = 직전 활성 Pass 출력.

			// [D-3] 출력이 있으면 lastResult 갱신. nullptr(Particle/ImGui 등 자기합성 안 함)이면 통과 유지.
			if (const Texture *result = p->GetPassResult())
				lastResult = result;

			if (DebugPassIndex == i)
				break; // 디버그: 이 Pass 까지만 실행 (PostFX 단계별 확인).
		}
		// BeforeIndex 필드는 미사용(lastResult 가 대체) - 명시 분기 배선이 필요할 때만 참조.
	}
}
