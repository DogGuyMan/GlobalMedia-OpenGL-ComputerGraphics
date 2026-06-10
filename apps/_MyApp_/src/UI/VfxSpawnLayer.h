/**
 * @file VfxSpawnLayer.h
 * @brief 로드된 Effekseer 이펙트를 드롭다운으로 선택하는 VFX 스폰 디버그 레이어.
 *
 * @details
 *  ### 책임
 *  - 주입된 @c Entry 목록에서 이펙트를 ImGui Combo 드롭다운으로 선택.
 *  - @c GetSelectedEffect() 를 통해 선택된 @c SJH::Effect* 를 caller(main) 에게 노출.
 *  - Game kind - F1 토글 없이 항상 렌더 (테스트 조작 UI).
 *
 *  ### 비-책임
 *  - [X] 이펙트 소폰 처리 - 좌클릭 Ground 콜백(main)이 GetSelectedEffect() 결과로 스폰.
 *  - [X] 이펙트 로드/소유 - @c ResourceRegistry 가 소유; 포인터만 참조.
 *  - [X] ImGui 컨텍스트 초기화 - @c UiBootstrap::BuildGameUI 담당.
 *
 * @note OnBuildUI 내부 ImGui 위젯 코드가 현재 주석 처리(비활성)되어 있음.
 *       활성화 시 "VFX Test" 창 + Combo 드롭다운 + 스폰 안내 문자열이 표시된다.
 */
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
	class Effect; // 포인터만 보유 - 전방 선언으로 충분 (Effekseer 헤더 불필요).
}

namespace TopdownShooter::UI
{
	/**
	 * @brief 로드된 Effekseer 이펙트를 Combo 드롭다운으로 선택하는 VFX 스폰 테스트 레이어.
	 * @details
	 *  - 선택된 이펙트는 @c GetSelectedEffect() - 좌클릭 Ground 소환 콜백(main)이 읽어 그 위치에 스폰.
	 *  - Game kind(항상 표시) - F1 토글 없이 바로 선택 가능.
	 *  - ImGui v1.53 핀 - @c Combo(label, &idx, const char* const items[], count) 오버로드 사용
	 *    (BeginCombo 도 1.53 에 있으나 items-배열 오버로드가 가장 안전).
	 *
	 *  현재 OnBuildUI 내부 위젯 코드는 주석 처리 상태 - 활성화 시 "VFX Test" 창이 열린다.
	 */
	class VfxSpawnLayer : public IImGuiLayer
	{
	  public:
		/**
		 * @brief 이펙트 항목 하나 - 표시 이름 + 이펙트 포인터 쌍.
		 * @details @c VfxSpawnLayer 생성자에 주입되며, 이펙트 소유권은 갖지 않는다.
		 */
		struct Entry
		{
			std::string  name;              ///< Combo 드롭다운에 표시될 이름.
			SJH::Effect *effect = nullptr;  ///< 비소유 포인터 - @c ResourceRegistry 가 소유.
		};

		/// @brief 생성자.
		/// @param entries 드롭다운에 열거할 이펙트 항목 목록 (std::move 로 내부 보유).
		explicit VfxSpawnLayer(std::vector<Entry> entries) : mEntries(std::move(entries))
		{
		}

		/// @brief Game kind 반환 - 항상 표시 (테스트 조작 UI).
		/// @return @c ImGuiLayerKind::Game.
		ImGuiLayerKind GetKind() const override
		{
			return ImGuiLayerKind::Game; // 항상 표시 (테스트 조작 UI).
		}

		/// @brief "VFX Test" 창 빌드 - 현재 내부 위젯 코드는 주석 처리 상태.
		void OnBuildUI() override
		{
			// ImGui::Begin("VFX Test");
			// if (mEntries.empty())
			// {
			// 	ImGui::TextUnformatted("(no effects loaded)");
			// 	ImGui::End();
			// 	return;
			// }

			// // std::string -> const char* 배열 (매 프레임, 항목 소수라 무해).
			// std::vector<const char *> names;
			// names.reserve(mEntries.size());
			// for (const auto &e : mEntries)
			// 	names.push_back(e.name.c_str());

			// ImGui::Combo("Effect", &mSelected, names.data(), static_cast<int>(names.size()));
			// ImGui::TextUnformatted("Left-click on ground to spawn");
			// ImGui::End();
		}

		/// @brief 현재 선택된 이펙트 포인터 반환.
		/// @return mSelected 범위가 유효하면 해당 Entry 의 effect 포인터, 아니면 nullptr.
		SJH::Effect *GetSelectedEffect() const
		{
			if (mSelected < 0 || mSelected >= static_cast<int>(mEntries.size()))
				return nullptr;
			return mEntries[static_cast<std::size_t>(mSelected)].effect;
		}

	  private:
		std::vector<Entry> mEntries;    ///< 드롭다운 항목 목록 (값 소유).
		int                mSelected = 0;  ///< 현재 선택된 항목 인덱스 (0-based).
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_UI_VFX_SPAWN_LAYER_H__