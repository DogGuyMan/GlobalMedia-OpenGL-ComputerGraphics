/**
 * @file PassDebugLayer.h
 * @brief PassIterator 에 등록된 Pass 들의 활성화(Enabled) 토글 + PostFX 파라미터 실시간 조정 디버그 레이어.
 *
 * @details
 *  ### 책임
 *  - @c PassIterator::Keys() 로 등록 Pass 를 열거하고, 각 Pass 의 @c Enabled 를 ImGui 체크박스로 토글.
 *    (접근법 B - 중앙 컬렉션 키 조회: Unity RenderGraph passName / Godot Compositor array 정통.
 *     Keys()/Find(key)/GetPassKey() 로 키 매칭. Find(key)->Enabled 단일 경로.)
 *  - 활성 PostFX 효과는 @c ResourceRegistry::FindSharedMaterial("mat_pass_<key>") 로 머티리얼을 조회해
 *    파라미터(gamma / fog / bloom / vignette)를 슬라이더로 직접 편집 (구 PostFXDebugLayer 복구, 2026-06-24).
 *    셰이더 uniform 은 draw 시점 mesh/postfx 경로가 @c Properties 에서 읽어 전송 (편집은 Properties 만 갱신).
 *  - grayscale 강도(@c uGrayscaleAmount)는 HP 가 구동하므로 read-only 표시만 (관찰 전용).
 *  - Editor kind - F1(showEditor) 토글 시에만 표시.
 *
 *  ### 비-책임
 *  - [X] Pass 소유/수명 - @c PassIterator 가 소유. 본 레이어는 비소유 포인터만 보유(접근은 Find).
 *  - [X] grayscale 값 *기록* - @c HpGrayscalePostFX(HP 비율) 담당. 여기선 표시만.
 *  - [X] 머티리얼 소유 - ResourceRegistry 가 소유. 조회만.
 *
 *  ### 정통 매핑
 *  - Unity Editor Inspector / Unreal Details Panel - 런타임 셰이더 파라미터 조정.
 *
 * @note 구조 Pass(Skybox/World/Particle)를 끄면 화면이 비거나 검게 되는 것은 정상 - 디버그 토글이라
 *       의도된 동작. present 를 끄면 backbuffer 합성이 사라져 검은 화면이 된다.
 *       머티리얼 키 컨벤션은 @c GetPassKey() == PostFX 효과명(PASS_*) 가정 - PostFxPass 의 키가
 *       "mat_pass_<key>" 의 <key> 와 일치해야 파라미터 위젯이 노출된다(이름 불일치 시 토글만 표시).
 */
#ifndef __MYAPP_UI_PASS_DEBUG_LAYER_H__
#define __MYAPP_UI_PASS_DEBUG_LAYER_H__

#include "Playable/Constants.h" // PASS_* 효과명
#include "UI/IImGuiLayer.h"
#include "material/material.h"                      // SJH::Material::Properties
#include "material/material_property_block.h"       // MaterialPropertyBlock (위젯 헬퍼 인자)
#include "render/pass_iterator.h"                   // PassIterator::Keys / Find
#include "render/render_passable/render_passable.h" // IPassable::Enabled
#include "resource_registry/resource_registry.h"    // FindSharedMaterial("mat_pass_<key>")

#include <imgui.h>
#include <spdlog/spdlog.h> // [Task 2.2 진단] SetPass 전후 상태 로깅
#include <string>

namespace TopdownShooter::UI
{
	/// @brief Pass 활성화 토글 + PostFX 파라미터 조정 디버그 레이어 (Editor kind).
	class PassDebugLayer : public IImGuiLayer
	{
	  public:
		/// @param iter         열거/조회 대상 PassIterator (비소유). nullptr 시 OnBuildUI no-op.
		/// @param grayscaleMat grayscale 공유 Material (비소유, 선택). nullptr 이면 강도 표시 생략.
		PassDebugLayer(SJH::PassIterator *iter)
		    : mIter(iter)
		{
			auto &reg = SJH::ResourceRegistry::Get();
			mGrayscaleMat = reg.FindSharedMaterial(std::string("mat_pass_") + Playable::PASS_GRAYSCALE_VIGNETTING);
		}

		void OnBuildUI() override
		{
			if (!mIter)
				return;

			auto &reg = SJH::ResourceRegistry::Get();

			ImGui::Begin("Pass Debug (F1)");
			ImGui::TextUnformatted("Pass Enabled + Params:");

			for (const auto &key : mIter->Keys())
			{
				auto *p = mIter->Find(key);
				if (!p)
					continue;

				ImGui::PushID(key.c_str()); // 패스마다 동일 위젯 라벨 ID 충돌 방지.
				ImGui::Checkbox(key.c_str(), &p->Enabled);

				// 활성 + 해당 효과 머티리얼이 있으면 파라미터 위젯 빌드(접근법 B - registry 키 직접 조회).
				if (p->Enabled)
				{
					if (auto *mat = reg.FindSharedMaterial("mat_pass_" + key))
						BuildPassParams(key, mat->Properties);
				}
				ImGui::PopID();
			}

			// ! 학습전용 코드
			// !	[ Phase Task 2.2] 학습 전용이므로 필요없으면 삭제하면 되는 대상
			// stage_wall 은 lazy 조회 - 씬에 아직/전혀 없으면 nullptr 이라 매 프레임 조회 + null 가드 (위 mat_pass 와 동일 패턴).
			static int queueChoice = 1;
			if (SJH::Material *wallMatPtr = reg.FindSharedMaterial("stage_wall"))
			{
				bool isChanged = false;
				isChanged |= ImGui::RadioButton("Hard cut (AlphaTest)", &queueChoice, 0);
				isChanged |= ImGui::RadioButton("Soft blend (Transparent)", &queueChoice, 1);
				isChanged |= ImGui::RadioButton("Soft + DepthWrite (Q6)", &queueChoice, 2);
				if (isChanged)
				{
					SJH::Pass::RenderQueue queue =
					    (queueChoice == 0)   ? SJH::Pass::RenderQueue::AlphaTest
					    : (queueChoice == 1) ? SJH::Pass::RenderQueue::Transparent
					                         : SJH::Pass::RenderQueue::TransparentDepthWrite;
					wallMatPtr->SetPass(queue);
					// [Task 2.2 진단] SetPass 직후 상태 로깅 - 공유 template vs 벽 인스턴스(실제 렌더 대상) 비교.
					//   가설: 벽은 stage_wall 을 clone 한 인스턴스(stage_wall_Wall*)로 렌더되므로, 공유 SetPass 가
					//   인스턴스 mState 를 안 바꿔 항상 Transparent(blend on) 로 보인다. -> 아래 Blend 값으로 확인.
					auto dumpMat = [](const char *label, SJH::Material *matPtr) {
						if (matPtr == nullptr)
						{
							spdlog::warn("[PassDebug] {} = <null>", label);
							return;
						}
						const auto &state = matPtr->GetRenderStateBlock();
						spdlog::info("[PassDebug] {:<22} ptr={} pass={} Blend={} DepthWrite={}",
						             label, static_cast<const void *>(matPtr),
						             static_cast<int>(matPtr->GetPass()), state.BlendEnable, state.DepthWrite);
					};
					spdlog::info("[PassDebug] ===== queueChoice={} -> SetPass(shared stage_wall) =====", queueChoice);
					dumpMat("shared stage_wall", wallMatPtr);
					dumpMat("inst WallTop", reg.FindMaterialInstance("stage_wall_WallTop"));
					dumpMat("inst WallBottom", reg.FindMaterialInstance("stage_wall_WallBottom"));
					dumpMat("inst WallLeft", reg.FindMaterialInstance("stage_wall_WallLeft"));
					dumpMat("inst WallRight", reg.FindMaterialInstance("stage_wall_WallRight"));
				}
			}

			// grayscale 강도 read-only - HP(HpGrayscalePostFX) 가 구동하므로 여기선 관찰만.
			if (mGrayscaleMat)
			{
				ImGui::Separator();
				const auto &floats = mGrayscaleMat->Properties.Floats;
				const auto it = floats.find("uGrayscaleAmount");
				const float amount = (it != floats.end()) ? it->second : -1.0f;
				ImGui::Text("uGrayscaleAmount = %.3f (HP-driven)", amount); // 1=color, 0=gray.
			}

			ImGui::End();
		}

		ImGuiLayerKind GetKind() const override
		{
			return ImGuiLayerKind::Editor;
		}

	  private:
		/// @brief 패스 이름별 전용 파라미터 위젯 빌드 - 구 PostFXDebugLayer 분기 이식.
		/// @details 편집은 @c props(Material::Properties) 만 갱신. grayscale 강도는 HP 구동이라 제외(위 read-only 표시).
		/// @param key   PostFX 효과명(@c PASS_*).
		/// @param props 해당 효과 머티리얼의 프로퍼티 백(슬라이더가 직접 수정).
		void BuildPassParams(const std::string &key, SJH::MaterialPropertyBlock &props)
		{
			if (key == Playable::PASS_GAMMA)
			{
				ImGui::SliderFloat("gamma", &props.Floats["gamma"], 0.1f, 2.5f);
			}
			else if (key == Playable::PASS_FOG)
			{
				ImGui::SliderFloat("density", &props.Floats["uFogDensity"], 0.0f, 0.5f);
				ImGui::SliderFloat("start", &props.Floats["uFogStart"], 0.0f, 1.0f);
				ImGui::SliderFloat("end", &props.Floats["uFogEnd"], 0.0f, 1.0f);
				ImGui::ColorEdit3("color", &props.Vec3s["uFogColor"][0]);
				ImGui::SliderInt("mode", &props.Ints["uFogMode"], 0, 2); // 0=Linear, 1=Exp, 2=Exp2
			}
			else if (key == Playable::PASS_BLOOM)
			{
				ImGui::SliderFloat("threshold", &props.Floats["uBloomThreshold"], 0.0f, 1.5f);
				ImGui::SliderFloat("spread", &props.Floats["uBloomSpread"], 0.1f, 5.0f);
				ImGui::SliderFloat("intensity", &props.Floats["uBloomIntensity"], 0.0f, 4.0f);
			}
			else if (key == Playable::PASS_GRAYSCALE_VIGNETTING)
			{
				// grayscale 강도는 HP 구동 - 여기선 vignette(색+강도)만 편집.
				ImGui::SliderFloat("vignette", &props.Floats["uVignetteAmount"], 0.0f, 1.0f);
				ImGui::ColorEdit3("vig color", &props.Vec3s["uVignetteColor"][0]);
			}
			else if (key == Playable::PASS_DEPTH_DEBUG)
			{
				// Phase1 학습 - linearize 0<->1 토글로 raw(비선형) vs 선형화 비교. near/far 는 카메라와 맞춰 튜닝.
				ImGui::SliderFloat("linearize", &props.Floats["uLinearize"], 0.0f, 1.0f);
				ImGui::SliderFloat("near", &props.Floats["uNear"], 0.01f, 5.0f);
				ImGui::SliderFloat("far", &props.Floats["uFar"], 10.0f, 500.0f);
			}
		}

		SJH::PassIterator *mIter = nullptr;     ///< 열거/조회 대상 (비소유).
		SJH::Material *mGrayscaleMat = nullptr; ///< grayscale 공유 Material (비소유, 선택 - read-only 표시용).
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_UI_PASS_DEBUG_LAYER_H__
