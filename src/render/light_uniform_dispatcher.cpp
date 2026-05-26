/**
 * @file light_uniform_dispatcher.cpp
 * @brief LightUniformDispatcher::Dispatch — Light uniform 일괄 송신 구현.
 */
#include "render/light_uniform_dispatcher.h"
#include "common/constants.h"
#include "object/light.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "render/device_context.h"
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace SJH
{
	void LightUniformDispatcher::Dispatch(const std::vector<Program *> &programs,
	                                      DirLight *dir,
	                                      const std::vector<PointLight *> &points,
	                                      const std::vector<SpotLight *> &spots,
	                                      const vmath::vec3 &viewPos)
	{
		if (static_cast<int>(points.size()) > Const::MAX_POINT_LIGHTS)
			spdlog::warn("LightUniformDispatcher — PointLight {} 개 발견. 셰이더 MAX_POINT_LIGHTS={} 초과분 무시.",
			             points.size(), Const::MAX_POINT_LIGHTS);
		if (static_cast<int>(spots.size()) > Const::MAX_SPOT_LIGHTS)
			spdlog::warn("LightUniformDispatcher — SpotLight {} 개 발견. 셰이더 MAX_SPOT_LIGHTS={} 초과분 무시.",
			             spots.size(), Const::MAX_SPOT_LIGHTS);

		auto &rc = DeviceContext::Get();
		for (const auto *prog : programs)
		{
			if (!prog)
				continue;
			// Lighting schema sentinel — UNI_VIEW_POS 가 UniformCache 에 없으면
			// 이 program 은 lighting 미사용 (simple/passthrough/postfx 등) → 송신 통째 skip.
			// warn-once 노이즈 차단 + glUseProgram 비용 회피.
			if (prog->GetLocation(Const::UNI_VIEW_POS) < 0)
				continue;
			rc.UseProgram(*prog);

			// viewPos — Phong specular 계산용.
			Uniforms::SetVec3(*prog, Const::UNI_VIEW_POS, viewPos);

			// DirLight — 1개. 없으면 enabled=0 만 전송 (uniform 0 보장).
			if (dir)
			{
				Uniforms::SetDirLight(*prog, Const::UNI_DIR_LIGHT, *dir, dir->GetWorldDirection());
				Uniforms::SetInt(*prog, Const::UNI_DIR_LIGHT_ENABLED, 1);
			}
			else
			{
				Uniforms::SetInt(*prog, Const::UNI_DIR_LIGHT_ENABLED, 0);
			}

			// PointLights — 최대 MAX_POINT_LIGHTS 개. 초과는 무시. 부족하면 enabled=0 으로 slot 채움.
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_POINT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_POINT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr  = Const::UNI_POINT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < points.size())
				{
					Uniforms::SetPointLight(*prog, idxStr.c_str(), *points[i], points[i]->GetWorldPosition());
					Uniforms::SetInt(*prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(*prog, enStr.c_str(), 0);
				}
			}

			// SpotLights — 최대 MAX_SPOT_LIGHTS 개. PointLights 와 완전 동일 패턴.
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_SPOT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr  = Const::UNI_SPOT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < spots.size())
				{
					Uniforms::SetSpotLight(*prog, idxStr.c_str(), *spots[i],
					                      spots[i]->GetWorldPosition(),
					                      spots[i]->GetWorldDirection());
					Uniforms::SetInt(*prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(*prog, enStr.c_str(), 0);
				}
			}
		}
	}

} // namespace SJH
