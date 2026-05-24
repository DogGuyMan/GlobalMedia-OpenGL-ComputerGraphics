// audio_demo/demo2 — 사용자 제작 bank 테스팅.
// 구성:
//   - Buses    : bus:/BGM Bus, bus:/SFX Bus
//   - Events   : event:/BGM (loop), event:/Damaged, event:/Slash (one-shot)
//   - Param    : BGM_STATE (event-instance, Labeled: Title/Combat/Boss),
//                Health    (global, Continuous 0..1)
#include <sb7.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include <fmod/fmod_common.h>
#include <fmod/fmod_errors.h>
#include <fmod/fmod_studio.hpp>
#include <fmod/fmod_studio_common.h>

#include <cstdio>
#include <cstdlib>

#ifdef __APPLE__
#include <libgen.h>      // dirname
#include <limits.h>      // PATH_MAX
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <stdint.h>      // uint32_t
#include <string.h>      // strncpy
#include <unistd.h>      // chdir
#endif

namespace
{
	// 학습 챕터 — 첫 실패에서 즉시 종료 (graceful degradation 안 함).
	void ck(FMOD_RESULT r, const char *where)
	{
		if (r != FMOD_OK)
		{
			std::fprintf(stderr, "[FMOD] %s: %s\n", where, FMOD_ErrorString(r));
			std::exit(1);
		}
	}
} // namespace

class audio_demo2_application : public sb7::application
{
	// Core
	FMOD::Studio::System *mSystem = nullptr;
	FMOD::Studio::Bank   *mMasterBank  = nullptr;
	FMOD::Studio::Bank   *mStringsBank = nullptr;

	// BGM — persistent loop event (Play/Pause/BGM_STATE 조작 대상)
	FMOD::Studio::EventDescription *mBgmDesc     = nullptr;
	FMOD::Studio::EventInstance    *mBgmInstance = nullptr;

	// SFX — one-shot 이벤트. description 만 캐시, 인스턴스는 버튼 클릭 시 생성->start->release.
	FMOD::Studio::EventDescription *mDamagedDesc = nullptr;
	FMOD::Studio::EventDescription *mSlashDesc   = nullptr;

	// Buses — BGM/SFX 그룹별 볼륨 제어
	FMOD::Studio::Bus *mBgmBus = nullptr;
	FMOD::Studio::Bus *mSfxBus = nullptr;

	// ImGui
	ImGuiContext *mImGuiCtx = nullptr;

	// UI 상태
	bool  mBgmPaused   = false;
	int   mBgmStateIdx = 0;     // 0=Title, 1=Combat, 2=Boss
	float mHealth      = 1.0f;  // global parameter
	float mBgmVolume   = 1.0f;
	float mSfxVolume   = 1.0f;

	void init() override
	{
		sb7::application::init();
		info.majorVersion = 4;
		info.minorVersion = 1;
		std::snprintf(info.title, sizeof(info.title), "FMOD Studio Audio Demo2 — Custom Bank");
#ifdef __APPLE__
		// macOS GLFW 3.0.4 는 glfwInit() 시 _GLFW_USE_CHDIR 로 CWD 를
		// 앱 번들 Resources 경로로 변경한다. bank 상대 경로를 살리기 위해
		// 실행 파일 디렉토리로 되돌린다.
		char exePath[PATH_MAX] = {};
		uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
		if (_NSGetExecutablePath(exePath, &exeSize) == 0)
		{
			char exePathCopy[PATH_MAX] = {};
			strncpy(exePathCopy, exePath, PATH_MAX - 1);
			chdir(dirname(exePathCopy));
		}
#endif
	}

	// One-shot SFX 헬퍼 — create -> start -> release.
	// release() 는 인스턴스를 즉시 파괴하지 않고, 재생이 끝나면 FMOD 가 자동 해제 (정통 one-shot 패턴).
	void play_one_shot(FMOD::Studio::EventDescription *desc, const char *label)
	{
		FMOD::Studio::EventInstance *inst = nullptr;
		ck(desc->createInstance(&inst), "one-shot createInstance");
		ck(inst->start(),               "one-shot start");
		ck(inst->release(),             "one-shot release");
		std::fprintf(stderr, "[demo2] one-shot: %s\n", label);
	}

	void startup() override
	{
		// System
		ck(FMOD::Studio::System::create(&mSystem), "System::create");
		ck(mSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
		                                           "System::initialize");
		std::fprintf(stderr, "[demo2] FMOD Studio system initialized.\n");

		// Banks — 사용자 제작 bank (Master + Strings 2개에 모든 컨텐츠 패킹)
		ck(mSystem->loadBankFile("resources/banks/Master.bank",
		                          FMOD_STUDIO_LOAD_BANK_NORMAL, &mMasterBank),
		                                           "loadBankFile Master");
		ck(mSystem->loadBankFile("resources/banks/Master.strings.bank",
		                          FMOD_STUDIO_LOAD_BANK_NORMAL, &mStringsBank),
		                                           "loadBankFile Strings");
		std::fprintf(stderr, "[demo2] Banks loaded.\n");

		// Events — BGM 은 persistent, SFX 는 description 만 (인스턴스는 클릭 시)
		ck(mSystem->getEvent("event:/BGM",     &mBgmDesc),     "getEvent BGM");
		ck(mSystem->getEvent("event:/Damaged", &mDamagedDesc), "getEvent Damaged");
		ck(mSystem->getEvent("event:/Slash",   &mSlashDesc),   "getEvent Slash");

		// BGM 인스턴스 생성 + 재생 (loop)
		ck(mBgmDesc->createInstance(&mBgmInstance), "BGM createInstance");
		ck(mBgmInstance->start(),                   "BGM start");
		std::fprintf(stderr, "[demo2] BGM playing.\n");

		// Buses — 공백 포함 이름 ("BGM Bus" / "SFX Bus") 그대로 사용
		ck(mSystem->getBus("bus:/BGM Bus", &mBgmBus), "getBus BGM Bus");
		ck(mSystem->getBus("bus:/SFX Bus", &mSfxBus), "getBus SFX Bus");
		std::fprintf(stderr, "[demo2] Buses ready (BGM Bus + SFX Bus).\n");

		// ImGui
		mImGuiCtx = ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplGlfwGL3_Init(window, true);
	}

	void render(double /*currentTime*/) override
	{
		if (mSystem)
		{
			ck(mSystem->update(), "System::update");
		}

		static const GLfloat clearColor[] = {0.10f, 0.10f, 0.12f, 1.0f};
		glClearBufferfv(GL_COLOR, 0, clearColor);

		ImGui_ImplGlfwGL3_NewFrame();
		ImGui::Begin("Audio Demo2 — Custom Bank");

		// 1) BGM Play / Pause
		ImGui::Text("BGM");
		if (ImGui::Button("Play"))
		{
			ck(mBgmInstance->start(), "BGM start");
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Pause", &mBgmPaused))
		{
			ck(mBgmInstance->setPaused(mBgmPaused), "BGM setPaused");
		}

		// 2) BGM_STATE — event-instance labeled (Title/Combat/Boss)
		static const char *kBgmStateLabels[] = {"Title", "Combat", "Boss"};
		if (ImGui::Combo("BGM_STATE", &mBgmStateIdx, kBgmStateLabels, 3))
		{
			ck(mBgmInstance->setParameterByName("BGM_STATE",
			                                    static_cast<float>(mBgmStateIdx)),
			                                    "setParameterByName BGM_STATE");
		}

		ImGui::Separator();

		// 3) Health — global parameter (System level, 모든 이벤트 공유)
		ImGui::Text("Global Parameter");
		if (ImGui::SliderFloat("Health", &mHealth, 0.0f, 1.0f, "%.2f"))
		{
			ck(mSystem->setParameterByName("Health", mHealth),
			                                    "setParameterByName Health");
		}

		ImGui::Separator();

		// 4) SFX one-shot — 버튼 클릭 시 즉시 create + start + release
		ImGui::Text("SFX (one-shot)");
		if (ImGui::Button("Damaged"))
		{
			play_one_shot(mDamagedDesc, "Damaged");
		}
		ImGui::SameLine();
		if (ImGui::Button("Slash"))
		{
			play_one_shot(mSlashDesc, "Slash");
		}

		ImGui::Separator();

		// 5) Bus 볼륨 — BGM Bus / SFX Bus 각각
		ImGui::Text("Bus Volume");
		if (ImGui::SliderFloat("BGM Bus", &mBgmVolume, 0.0f, 1.0f, "%.2f"))
		{
			ck(mBgmBus->setVolume(mBgmVolume), "BGM Bus setVolume");
		}
		if (ImGui::SliderFloat("SFX Bus", &mSfxVolume, 0.0f, 1.0f, "%.2f"))
		{
			ck(mSfxBus->setVolume(mSfxVolume), "SFX Bus setVolume");
		}

		ImGui::End();
		ImGui::Render();
	}

	void shutdown() override
	{
		if (mImGuiCtx)
		{
			ImGui_ImplGlfwGL3_Shutdown();
			ImGui::DestroyContext(mImGuiCtx);
			mImGuiCtx = nullptr;
		}

		if (mBgmInstance)
		{
			mBgmInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mBgmInstance->release();
			mBgmInstance = nullptr;
		}
		// EventDescription 들은 Bank 소유 — 명시적 해제 없이 포인터만 null
		mBgmDesc     = nullptr;
		mDamagedDesc = nullptr;
		mSlashDesc   = nullptr;

		if (mStringsBank) { mStringsBank->unload(); mStringsBank = nullptr; }
		if (mMasterBank)  { mMasterBank->unload();  mMasterBank  = nullptr; }

		if (mSystem)
		{
			ck(mSystem->release(), "System::release");
			mSystem = nullptr;
		}
	}
};

DECLARE_MAIN(audio_demo2_application);
