#ifndef __ENGINE_TRANSFORM_H__
#define __ENGINE_TRANSFORM_H__

#include "vmath.h"
#include <string>
#include <unordered_map>

namespace Engine::Transform
{
	class Transform
	{
	  public:
		std::string Name;

		vmath::vec3 Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 EulerRot = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

		Transform *Parent = nullptr;
		std::unordered_map<std::string, Transform *> Children;

		vmath::mat4 GetModelMatrix() const
		{
			vmath::mat4 local =
			    vmath::translate<float>(Translate) *
			    vmath::rotate<float>(EulerRot[2], 0.0f, 0.0f, 1.0f) *
			    vmath::rotate<float>(EulerRot[1], 0.0f, 1.0f, 0.0f) *
			    vmath::rotate<float>(EulerRot[0], 1.0f, 0.0f, 0.0f) *
			    vmath::scale<float>(Scale);
			if (Parent == nullptr)
				return local;
			return Parent->GetModelMatrix() * local;
		}
	};
} // namespace Engine::Transform

#endif // __ENGINE_TRANSFORM_H__
