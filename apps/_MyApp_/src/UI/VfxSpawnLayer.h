#ifndef __MYAPP_UI_VFX_SPAWN_LAYER_H__
#define __MYAPP_UI_VFX_SPAWN_LAYER_H__

#include "UI/IImGuiLayer.h"
#include <cstddef>
#include <imgui.h>
#include <string>
#include <utility>
#include <vector>

namespace SJH
{
	class Effect; // 포인터만 보유 — 전방 선언으로 충분 (Effekseer 헤더 불필요).
}

namespace TopdownShooter::UI
{
	/// @brief 로드된 Effekseer 이펙트를 드롭다운으로 고르는 테스트 레이어.
	/// @details
	///   - 선택된 이펙트는 `GetSelectedEffect()` — 좌클릭 Ground 소환 콜백(main)이 읽어 그 위치에 스폰.
	///   - Game kind(항상 표시) — 토글 없이 바로 선택 가능.
	///   - ImGui v1.53 핀 — `Combo(label, &idx, const char* const items[], count)` 오버로드 사용
	///     (BeginCombo 도 1.53 에 있으나 items-배열 오버로드가 가장 안전).
	class VfxSpawnLayer : public IImGuiLayer
	{
	  public:
		struct Entry
		{
			std::string  name;
			SJH::Effect *effect = nullptr;
		};

		explicit VfxSpawnLayer(std::vector<Entry> entries) : mEntries(std::move(entries))
		{
		}

		ImGuiLayerKind GetKind() const override
		{
			return ImGuiLayerKind::Game; // 항상 표시 (테스트 조작 UI).
		}

		void OnBuildUI() override
		{
			ImGui::Begin("VFX Test");
			if (mEntries.empty())
			{
				ImGui::TextUnformatted("(no effects loaded)");
				ImGui::End();
				return;
			}

			// std::string -> const char* 배열 (매 프레임, 항목 소수라 무해).
			std::vector<const char *> names;
			names.reserve(mEntries.size());
			for (const auto &e : mEntries)
				names.push_back(e.name.c_str());

			ImGui::Combo("Effect", &mSelected, names.data(), static_cast<int>(names.size()));
			ImGui::TextUnformatted("Left-click on ground to spawn");
			ImGui::End();
		}

		/// @brief 현재 선택된 이펙트 (없으면 nullptr).
		SJH::Effect *GetSelectedEffect() const
		{
			if (mSelected < 0 || mSelected >= static_cast<int>(mEntries.size()))
				return nullptr;
			return mEntries[static_cast<std::size_t>(mSelected)].effect;
		}

	  private:
		std::vector<Entry> mEntries;
		int                mSelected = 0;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_UI_VFX_SPAWN_LAYER_H__
