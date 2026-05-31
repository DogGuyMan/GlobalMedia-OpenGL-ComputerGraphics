#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
#include "BaseEntity.h"
#include "Components/Components.Interfaces.h"
#include "Components/MovementComponents.h"
#include "Components/WeaponComponents.h"
#include "InputHandler/PlayerController.h"
#include "input/mouse_input.h"
#include "scene/actor.h"
#include "sprite/sprite_component.h"
#include <cmath>
#include <vmath.h>

namespace TopdownShooter::Entity
{
	class PlayerSingleHand : public SJH::Scene::Component
	{
	  private:
		SJH::Sprite::SpriteRenderer mHandSprite;
		float yOffset;
		float maxRadius;
		float minRadius;

	  public:
		PlayerSingleHand();
		float GetRadius(vmath::vec2 forwardVector)
		{
			auto fLen = vmath::length(forwardVector);
			if (fLen > maxRadius)
				return maxRadius;
			if (fLen < minRadius)
				return minRadius;
			return fLen;
		};
		void OrbitWithYAngle(vmath::vec2 forwardVector)
		{
			auto &transform = this->GetOwner()->GetTransform();
			float forwardAngle = 0; // acos(forwardVector)???;
			transform.Translate = vmath::vec3(
			    cos(forwardAngle) * GetRadius(forwardVector),
			    yOffset,
			    sin(forwardAngle) * GetRadius(forwardVector));
		};

		virtual void OnEnter() override {

		};
		virtual void OnExit() override {

		};
		virtual void Update() override {

		};
	};

	class PlayerHands : public SJH::Scene::Component
	{
	  private:
		PlayerSingleHand mLeftHand;
		PlayerSingleHand mRightHand;

	  public:
		PlayerHands();
		virtual void OnEnter() override {

		};
		virtual void OnExit() override {

		};
		
		virtual void Update() override {

		};
	};
}; // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__