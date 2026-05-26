#ifndef _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__
#define _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__

#include <Effekseer.h>
#include <EffekseerRendererGL.h>

namespace TopdownShooter::VFX
{
	/// @brief Effekseer Manager + EffekseerRendererGL Renderer owner.
	/// @details Director 멤버로 거주. Update(dt) 는 main update 단계, Draw(view, proj) 는 render 단계.
	class VFXSystem
	{
	  public:
		VFXSystem()  = default;
		~VFXSystem() = default;
		VFXSystem(const VFXSystem &)            = delete;
		VFXSystem &operator=(const VFXSystem &) = delete;

		void Init(int maxSprites = 8000);
		void Update(float dt);   // manager_->Update(dt * 60.0f)
		void Draw(const float *viewMat, const float *projMat);
		void Shutdown();

		::Effekseer::ManagerRef            GetManager()  { return mManager; }
		::EffekseerRendererGL::RendererRef GetRenderer() { return mRenderer; }

	  private:
		::EffekseerRendererGL::RendererRef mRenderer;
		::Effekseer::ManagerRef            mManager;
	};
}

#endif // _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__
