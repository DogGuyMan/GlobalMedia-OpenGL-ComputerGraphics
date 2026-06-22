/**
 * @file pass_iterator.cpp
 * @brief PassIterator::Execute - before/GetPassResult 체이닝 + bounds check + DebugPassIndex.
 */
#include "render/pass_iterator.h"
#include "render/render_passable/render_passable.h" // IPassable (Draw/GetPassResult/BeforeIndex/OnResize)

namespace SJH
{
	int PassIterator::Add(IPassable *pass)
	{
		mPasses.push_back(pass);
		return static_cast<int>(mPasses.size()) - 1;
	}

	void PassIterator::Resize(int w, int h)
	{
		for (auto *p : mPasses)
			if (p)
				p->OnResize(w, h);
	}

	void PassIterator::Execute(DeviceContext &rec, RenderTarget & /*backbuffer*/)
	{
		const int n = static_cast<int>(mPasses.size());
		for (int i = 0; i < n; ++i)
		{
			IPassable *p = mPasses[i];
			if (!p)
				continue;

			// BeforeIndex 가 가리키는 선행 Pass 의 출력 텍스처를 before 로 주입(bounds check).
			// -1/범위밖 = nullptr (scene raw - 첫 Pass 등). 현재 대부분 Pass 는 before 미소비(사전배선 사용).
			const int      bi     = p->BeforeIndex;
			const Texture *before = (bi >= 0 && bi < n && mPasses[bi]) ? mPasses[bi]->GetPassResult() : nullptr;

			p->Draw(rec, before);

			if (DebugPassIndex == i)
				break; // 디버그: 이 Pass 까지만 실행 (PostFX 단계별 확인).
		}
		// 순환 검사 불요 - BeforeIndex 는 항상 앞선 인덱스(Add 코스 순서)를 가리킨다는 계약.
	}
}
