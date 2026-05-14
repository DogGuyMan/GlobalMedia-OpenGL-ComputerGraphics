#include "engine/resource_management.h"

#include "diagnostics/engine_diagnostics.h"

#include <cstdlib>
#include <iostream>
#include <utility>

namespace diag = SJH::Diagnostics;

namespace Engine::ResourceManagement
{
	void ResourceManagement::AddModel(const std::string &name,
	                                  std::unique_ptr<Model::ModelBase> model,
	                                  Transform::Transform *parent)
	{
		if (Models.count(name) > 0)
		{
			std::cerr << "AddModel 중복 키 무시됨 : " << name << std::endl;
			exit(1);
		}

		Transform::Transform &transform = model->GetTransform();
		transform.Name = name;
		transform.Parent = parent;
		if (parent != nullptr)
		{
			parent->Children[name] = &transform;
			diag::EngineDiagnostics::CheckNoParentCycle(
			    &transform,
			    [](const Transform::Transform *t) { return t->Parent; },
			    name.c_str());
		}
		Models.insert(std::make_pair(name, std::move(model)));
	}

	Model::ModelBase *ResourceManagement::GetModel(const std::string &name)
	{
		auto it = Models.find(name);
		return (it == Models.end()) ? nullptr : it->second.get();
	}

	bool ResourceManagement::HasModel(const std::string &name) const
	{
		return Models.count(name) > 0;
	}

	void ResourceManagement::RemoveModel(const std::string &name)
	{
		auto it = Models.find(name);
		if (it == Models.end())
			return;

		Transform::Transform &transform = it->second->GetTransform();
		if (transform.Parent != nullptr)
			transform.Parent->Children.erase(name);

		Models.erase(it);
	}

	Material::Material *ResourceManagement::AddMaterial(const std::string &name,
	                                                    std::unique_ptr<Material::Material> mat)
	{
		if (Materials.count(name) > 0)
		{
			std::cerr << "AddMaterial 중복 키 무시됨 : " << name << std::endl;
			exit(1);
		}
		auto *raw = mat.get();
		Materials.insert(std::make_pair(name, std::move(mat)));
		return raw;
	}

	Material::Material *ResourceManagement::GetMaterial(const std::string &name)
	{
		auto it = Materials.find(name);
		return (it == Materials.end()) ? nullptr : it->second.get();
	}

	void ResourceManagement::TeardownGL()
	{
		Materials.clear();
		Models.clear();
	}
} // namespace Engine::ResourceManagement
