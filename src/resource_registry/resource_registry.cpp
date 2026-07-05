/**
 * @file resource_registry.cpp
 * @brief @c ResourceRegistry 메서드 정의 - 캐시 miss/hit 경로 구현.
 *
 * @details
 *  ### 구현 원칙
 *  - @c Create* / @c Register* - 캐시-미스 경로에서 새 자원을 생성/등록.
 *    @c emplace 결과 iterator 로 raw 포인터를 꺼내 반환 - 매니저 보관 인스턴스를 가리키므로
 *    호출자에게 노출되는 lifetime 은 매니저 자신의 lifetime 과 동일하다.
 *  - @c Find* - 캐시-히트 경로에서 기존 인스턴스를 즉시 반환.
 *  - Image 는 스코프 한정 - GPU 업로드 후 @c Create* 스택 프레임을 벗어나면 즉시 소멸.
 *
 *  ### FMOD 옵셔널 컴파일 가드
 *  @c CreateSound 는 @c SJH_HAS_FMOD 가드 - FMOD 미설치 환경에서도 링크 에러 없이 스텁 반환.
 *  @c CreateEffect 는 Effekseer 헤더가 항상 포함되므로 가드 없음.
 */
#include "resource_registry.h"
#ifdef SJH_HAS_FMOD
#include <fmod/fmod.hpp>      // M5 - CreateSound 의 createSound 호출
#endif
#include <spdlog/spdlog.h>

namespace SJH
{
	ResourceRegistry &ResourceRegistry::Get()
	{
		static ResourceRegistry instance;
		return instance;
	}

	ResourceRegistry::~ResourceRegistry()
	{
		Clear();
	}

	Texture *ResourceRegistry::CreateTexture(const std::string &key, const Image *image)
	{
		if (mTextures.find(key) != mTextures.end())
		{
			spdlog::warn("CreateTexture: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto texture = Texture::CreateTexture(image);
		if (texture == nullptr)
		{
			spdlog::error("CreateTexture: GPU 텍스처 생성 실패 - key '{}'", key);
			return nullptr;
		}
		auto insertedIt = mTextures.emplace(key, std::move(texture)).first;
		return insertedIt->second.get();
	}

	Texture *ResourceRegistry::FindTexture(const std::string &key)
	{
		auto it = mTextures.find(key);
		return (it != mTextures.end()) ? it->second.get() : nullptr;
	}

	Material *ResourceRegistry::CreateSharedMaterial(const std::string &key)
	{
		// 텍스처 독립 - 빈 Material 만 생성,캐시. 텍스처/프로그램 배선은 호출자 책임.
		if (mSharedMaterials.find(key) != mSharedMaterials.end())
		{
			spdlog::warn("CreateSharedMaterial: 키 '{}' 가 이미 존재 - FindSharedMaterial 을 먼저 호출하라", key);
			return nullptr;
		}
		auto insertedIt = mSharedMaterials.emplace(key, Material::Create()).first;
		return insertedIt->second.get();
	}

	Material *ResourceRegistry::FindSharedMaterial(const std::string &key)
	{
		auto it = mSharedMaterials.find(key);
		return (it != mSharedMaterials.end()) ? it->second.get() : nullptr;
	}

	Material *ResourceRegistry::CreateMaterialInstanceFrom(const std::string &key, const Material *template_)
	{
		if (!template_)
		{
			spdlog::warn("CreateMaterialInstanceFrom: 키 '{}' template_ 가 nullptr - 거부", key);
			return nullptr;
		}
		if (mMaterialInstances.find(key) != mMaterialInstances.end())
		{
			spdlog::warn("CreateMaterialInstanceFrom: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		// template_->Clone() 이 IsInstance=true + OriginalMaterial=template_ 자동 설정 (Material::Clone).
		auto instance = template_->Clone();
		auto insertedIt = mMaterialInstances.emplace(key, std::move(instance)).first;
		return insertedIt->second.get();
	}

	Material *ResourceRegistry::FindMaterialInstance(const std::string &key)
	{
		auto it = mMaterialInstances.find(key);
		return (it != mMaterialInstances.end()) ? it->second.get() : nullptr;
	}

	Model *ResourceRegistry::CreateModel(const std::string &key, const std::string &filename)
	{
		if (mModels.find(key) != mModels.end())
		{
			spdlog::warn("CreateModel: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto model = Model::Load(filename);
		if (model == nullptr)
		{
			spdlog::error("CreateModel: 모델 로드 실패 - key '{}', file '{}'", key, filename);
			return nullptr;
		}
		auto insertedIt = mModels.emplace(key, std::move(model)).first;
		return insertedIt->second.get();
	}

	Model *ResourceRegistry::FindModel(const std::string &key)
	{
		auto it = mModels.find(key);
		return (it != mModels.end()) ? it->second.get() : nullptr;
	}

	Program *ResourceRegistry::CreateProgram(const std::string &key,
	                                         const std::string &vertShaderFilename,
	                                         const std::string &fragShaderFilename)
	{
		if (mPrograms.find(key) != mPrograms.end())
		{
			spdlog::warn("CreateProgram: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto program = Program::CreateWithVSFS(vertShaderFilename, fragShaderFilename);
		if (program == nullptr)
		{
			spdlog::error("CreateProgram: 셰이더 컴파일/링크 실패 - key '{}', vs '{}', fs '{}'",
			              key, vertShaderFilename, fragShaderFilename);
			return nullptr;
		}
		auto insertedIt = mPrograms.emplace(key, std::move(program)).first;
		return insertedIt->second.get();
	}

	Program *ResourceRegistry::FindProgram(const std::string &key)
	{
		auto it = mPrograms.find(key);
		return (it != mPrograms.end()) ? it->second.get() : nullptr;
	}

	std::vector<Program *> ResourceRegistry::GetAllPrograms() const
	{
		std::vector<Program *> out;
		out.reserve(mPrograms.size());
		for (const auto &[key, prog] : mPrograms)
			out.push_back(prog.get());
		return out;
	}

	Mesh *ResourceRegistry::RegisterMesh(const std::string &key, MeshUPtr mesh)
	{
		if (mesh == nullptr)
		{
			spdlog::warn("RegisterMesh: 키 '{}' 에 nullptr Mesh 위탁 요청 - 거부", key);
			return nullptr;
		}
		if (mMeshes.find(key) != mMeshes.end())
		{
			spdlog::warn("RegisterMesh: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto insertedIt = mMeshes.emplace(key, std::move(mesh)).first;
		return insertedIt->second.get();
	}

	Mesh *ResourceRegistry::FindMesh(const std::string &key)
	{
		auto it = mMeshes.find(key);
		return (it != mMeshes.end()) ? it->second.get() : nullptr;
	}

	RenderTexture *ResourceRegistry::CreateRenderTexture(const std::string &key, int width, int height)
	{
		if (mRenderTextures.find(key) != mRenderTextures.end())
		{
			spdlog::warn("CreateRenderTexture: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto fb = RenderTexture::Create(width, height);
		if (fb == nullptr)
		{
			spdlog::error("CreateRenderTexture: FBO 생성 실패 - key '{}', {}x{}", key, width, height);
			return nullptr;
		}
		auto insertedIt = mRenderTextures.emplace(key, std::move(fb)).first;
		return insertedIt->second.get();
	}

	RenderTexture *ResourceRegistry::FindRenderTexture(const std::string &key)
	{
		auto it = mRenderTextures.find(key);
		return (it != mRenderTextures.end()) ? it->second.get() : nullptr;
	}

	Sprite::UniformAtlas *ResourceRegistry::CreateUniformAtlas(const std::string &key,
	                                                          const std::string &pngPath,
	                                                          int cols, int rows)
	{
		if (mAtlas.find(key) != mAtlas.end())
		{
			spdlog::warn("CreateUniformAtlas: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto atlas = std::make_unique<Sprite::UniformAtlas>();
		atlas->LoadFromPNG(pngPath.c_str()).SetGrid(cols, rows);
		if (!atlas->IsValid())
		{
			spdlog::error("CreateUniformAtlas: PNG 로드/grid 검증 실패 - key '{}', png '{}', grid {}x{}",
			              key, pngPath, cols, rows);
			return nullptr;
		}
		auto insertedIt = mAtlas.emplace(key, std::move(atlas)).first;
		return insertedIt->second.get();
	}

	Sprite::UniformAtlas *ResourceRegistry::FindUniformAtlas(const std::string &key)
	{
		auto it = mAtlas.find(key);
		return (it != mAtlas.end()) ? it->second.get() : nullptr;
	}

	// M5 - FMOD Sound
	Sound *ResourceRegistry::CreateSound(::FMOD::System *sys, const std::string &key, const std::string &path)
	{
		if (!sys)
		{
			spdlog::error("[ResourceRegistry::CreateSound] sys=nullptr (key={})", key);
			return nullptr;
		}
		if (mSounds.find(key) != mSounds.end())
		{
			spdlog::warn("[ResourceRegistry::CreateSound] key 중복: {}", key);
			return nullptr;
		}

#ifdef SJH_HAS_FMOD
		::FMOD::Sound *raw = nullptr;
		FMOD_RESULT r = sys->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &raw);
		if (r != FMOD_OK || !raw)
		{
			spdlog::error("[ResourceRegistry::CreateSound] createSound 실패 path={} FMOD_RESULT={}", path, int(r));
			return nullptr;
		}

		auto sound = std::make_unique<Sound>(raw);
		Sound *ret = sound.get();
		mSounds.emplace(key, std::move(sound));
		return ret;
#else
		(void)path;
		spdlog::warn("[ResourceRegistry::CreateSound] FMOD 미빌드 - nullptr 스텁 (key={})", key);
		return nullptr;
#endif
	}

	Sound *ResourceRegistry::FindSound(const std::string &key)
	{
		auto it = mSounds.find(key);
		return (it != mSounds.end()) ? it->second.get() : nullptr;
	}

	// M5 - Effekseer Effect
	Effect *ResourceRegistry::CreateEffect(::Effekseer::ManagerRef manager, const std::string &key, const char16_t *path)
	{
		// * Effekseer::RefPtr 은 operator! / operator bool 미지원 - Get() 으로 nullptr 비교
		if (manager.Get() == nullptr)
		{
			spdlog::error("[ResourceRegistry::CreateEffect] manager=null (key={})", key);
			return nullptr;
		}
		if (mEffects.find(key) != mEffects.end())
		{
			spdlog::warn("[ResourceRegistry::CreateEffect] key 중복: {}", key);
			return nullptr;
		}

		::Effekseer::EffectRef ref = ::Effekseer::Effect::Create(manager, reinterpret_cast<const EFK_CHAR *>(path));
		if (ref.Get() == nullptr)
		{
			spdlog::error("[ResourceRegistry::CreateEffect] Effekseer::Effect::Create 실패 (key={})", key);
			return nullptr;
		}

		auto eff = std::make_unique<Effect>(ref);
		Effect *ret = eff.get();
		mEffects.emplace(key, std::move(eff));
		return ret;
	}

	Effect *ResourceRegistry::FindEffect(const std::string &key)
	{
		auto it = mEffects.find(key);
		return (it != mEffects.end()) ? it->second.get() : nullptr;
	}

	void ResourceRegistry::Clear()
	{
		// * SP-MaterialMetadata - Material 의 OriginalMaterial dangling 차단:
		//   Instance 가 *항상 Shared 보다 먼저* 소멸하도록 명시 순서 (Unreal `UMaterialInstanceDynamic::Parent` 안전).
		mMaterialInstances.clear(); // * Instance 먼저 - OriginalMaterial 참조 객체들 소멸
		mSharedMaterials.clear();   // * Shared 나중 - 참조 대상 소멸
		mTextures.clear();
		mModels.clear();
		mPrograms.clear();
		mMeshes.clear();
		mRenderTextures.clear();
		mAtlas.clear();
		mSounds.clear();    // M5 - Sound dtor 가 FMOD::Sound::release() 호출
		mEffects.clear();   // M5 - EffectRef shared_ptr 자동 정리
	}
} // namespace SJH
