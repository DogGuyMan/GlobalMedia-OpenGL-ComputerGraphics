#ifndef __ENGINE_CAMERA_H__
#define __ENGINE_CAMERA_H__

#include "engine/transform.h"
#include "vmath.h"

namespace Engine::Camera
{
	class Camera
	{
	  public:
		Transform::Transform Transform;

		vmath::vec3 Target = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 WorldUp = vmath::vec3(0.0f, 1.0f, 0.0f);

		float Fov = 60.0f;
		float Aspect = 1.0f;
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;

		vmath::mat4 GetViewMatrix() const
		{
			return vmath::lookat(Transform.Translate, Target, WorldUp);
		}

		vmath::mat4 GetProjMatrix() const
		{
			return vmath::perspective(Fov, Aspect, NearPlane, FarPlane);
		}
	};
} // namespace Engine::Camera

#endif // __ENGINE_CAMERA_H__
