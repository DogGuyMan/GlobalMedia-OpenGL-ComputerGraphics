#include "scene/compound_actor.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "object/light.h"
#include "object/transform.h"
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vmath.h>

namespace SJH::Scene
{
    namespace
    {
        /// @brief direction vector  Transform.EulerRot (degree).
        /// @details OpenGL 정통 — Forward = -Z, Up = +Y. EulerRot=(pitch, yaw, 0) 의 ZYX 합성:
        ///   - pitch = asin(d.y)
        ///   - yaw   = atan2(d.x, -d.z)
        ///   - roll  = 0
        /// EulerRot=(0,0,0) 인 Transform 의 GetForward() = (0,0,-1) 와 일관.
        vmath::vec3 DirectionToEulerDeg(const vmath::vec3& dir)
        {
            constexpr float RAD2DEG = 57.295779513f;
            const auto d = vmath::normalize(dir);
            const float pitchRad = asinf(d[1]);
            const float yawRad   = atan2f(d[0], -d[2]);
            return vmath::vec3(pitchRad * RAD2DEG, yawRad * RAD2DEG, 0.0f);
        }
    }

    std::unique_ptr<Actor> CreateCameraActor(
        std::string name, float fovYDeg, float aspect, float nearZ, float farZ)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->AddComponent<Camera>(fovYDeg, aspect, nearZ, farZ);
        return actor;
    }

    std::unique_ptr<Actor> CreateDirLightActor(std::string name, vmath::vec3 direction)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->GetTransform().EulerRot = DirectionToEulerDeg(direction);
        actor->AddComponent<DirLight>();
        return actor;
    }

    std::unique_ptr<Actor> CreatePointLightActor(
        std::string name, vmath::vec3 position, float distance)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->GetTransform().Translate = position;
        auto* light = actor->AddComponent<PointLight>();
        light->Distance = distance;
        return actor;
    }

    std::unique_ptr<Actor> CreateSpotLightActor(
        std::string name, vmath::vec3 position, vmath::vec3 direction,
        float innerCutoffDeg, float outerCutoffDeg)
    {
        auto actor = std::make_unique<Actor>(std::move(name));
        actor->GetTransform().Translate = position;
        actor->GetTransform().EulerRot = DirectionToEulerDeg(direction);
        auto* light = actor->AddComponent<SpotLight>();
        light->CutoffAngleDeg       = innerCutoffDeg;
        light->OuterCutoffAngleDeg  = outerCutoffDeg;
        return actor;
    }
} // namespace SJH::Scene
