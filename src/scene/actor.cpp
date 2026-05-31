#include "scene/actor.h"
#include <algorithm>
#include <memory>
#include <type_traits>

// SP3 — Actor/Component 비복사,비이동 컴파일 타임 검증
static_assert(!std::is_copy_constructible_v<SJH::Scene::Actor>,
              "SJH::Scene::Actor must be non-copy-constructible");
static_assert(!std::is_move_constructible_v<SJH::Scene::Actor>,
              "SJH::Scene::Actor must be non-move-constructible");
static_assert(std::has_virtual_destructor_v<SJH::Scene::Component>,
              "SJH::Scene::Component must have virtual destructor (polymorphic base)");

namespace SJH::Scene
{
	Actor::Actor(std::string name) : mName(std::move(name))
	{
	}
	Actor::~Actor() = default;

	Actor *Actor::AddChild(std::unique_ptr<Actor> child)
	{
		if (!child)
			return nullptr; // ddd Early Return — nullptr 방어
		child->mParent = this;
		Actor *raw = child.get();
		mChildren.push_back(std::move(child));
		if (mEntered)
			raw->OnEnter();
		return raw;
	}

	void Actor::RemoveChild(Actor *child)
	{
		auto it = std::find_if(mChildren.begin(), mChildren.end(),
		                       [&](const auto &p) { return p.get() == child; });
		if (it == mChildren.end())
			return;
		if (mEntered)
			(*it)->OnExit();
		mChildren.erase(it);
	}

	std::unique_ptr<Actor> Actor::DetachChild(Actor *child)
	{
		auto it = std::find_if(mChildren.begin(), mChildren.end(),
		                       [&](const auto &p) { return p.get() == child; });
		if (it == mChildren.end())
			return nullptr;
		if (mEntered)
			(*it)->OnExit();
		(*it)->mParent = nullptr;
		auto uptr = std::move(*it);
		mChildren.erase(it);
		return uptr;
	}

	Actor *Actor::FindChild(const std::string &name, bool recursive) const
	{
		for (auto &child : mChildren)
		{
			if (child->mName == name)
				return child.get();
			if (recursive)
				if (auto *found = child->FindChild(name, true))
					return found;
		}
		return nullptr;
	}

	void Actor::RemoveAllComponents()
	{
		// ddd POLA: OnExit 는 enabled 무관 cleanup hook. mEntered 일 때만 호출.
		if (mEntered)
			for (auto &[_, comp] : mComponents)
				comp->OnExit();
		mComponents.clear();
	}

	vmath::mat4 Actor::GetWorldMatrix() const
	{
		const vmath::mat4 local = mTransform.GetLocalMatrix();
		if (mParent)
		{
			// vmath matNM::operator* 는 base 타입 반환 -> Tmat4(const base&) 로 명시 변환
			return vmath::mat4(mParent->GetWorldMatrix() * local);
		}
		return local;
	}

	void Actor::OnEnter()
	{
		if (mEntered)
			return;
		mEntered = true;
		for (auto &[_, comp] : mComponents)
			comp->OnEnter();
		for (auto &child : mChildren)
			child->OnEnter();
	}

	void Actor::OnExit()
	{
		if (!mEntered)
			return;
		for (auto it = mChildren.rbegin(); it != mChildren.rend(); ++it)
			(*it)->OnExit();
		for (auto &[_, comp] : mComponents)
			comp->OnExit();
		mEntered = false;
	}

	void Actor::Update(float dt)
	{
		if (!mActive)
			return;
		for (auto &[_, comp] : mComponents)
			if (comp->IsEnabled())
				comp->Update(dt);
		for (auto &child : mChildren)
			child->Update(dt);
	}
} // namespace SJH::Scene
