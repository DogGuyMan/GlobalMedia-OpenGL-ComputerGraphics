// audio_demo — FMOD Studio + ImGui 파라미터 데모 (single-file chapter).
// Task 9: Transport(Play/Stop/Pause) + Master 볼륨 컨트롤.
#include <sb7.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include <fmod/fmod_common.h>
#include <fmod/fmod_errors.h>
#include <fmod/fmod_studio.hpp>
#include <fmod/fmod_studio_common.h>

#include "common/common.h" // SJH::CrossPlatformDir

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

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
	struct ParamCache
	{
		std::string name;
		FMOD_STUDIO_PARAMETER_ID id;
		enum Kind
		{
			Continuous,
			Labeled,
			Switch,
			DiscreteInt
		};
		Kind kind;
		float minValue;
		float maxValue;
		std::vector<std::string> labels; // Labeled 일 때만
		float currentValue;
	};
} // namespace

class audio_demo_application : public sb7::application
{
	FMOD::Studio::System *mSystem = nullptr;
	FMOD::Studio::Bank *mMasterBank = nullptr;
	FMOD::Studio::Bank *mStringsBank = nullptr;
	FMOD::Studio::Bank *mMusicBank = nullptr;

	FMOD::Studio::EventDescription *mEventDesc = nullptr;
	FMOD::Studio::EventInstance *mInstance = nullptr;

	std::vector<ParamCache> mParams;
	ImGuiContext *mImGuiCtx = nullptr;
	FMOD::Studio::Bus *mMasterBus = nullptr;
	float mMasterVolume = 1.0f;
	bool mPaused = false;

	void init() override
	{
		sb7::application::init();
		info.majorVersion = 4;
		info.minorVersion = 1;
		std::snprintf(info.title, sizeof(info.title), "FMOD Studio + ImGui Audio Demo");
		// macOS GLFW chdir workaround — SJH::common 흡수 (bank 상대 경로 정합)
		SJH::CrossPlatformDir();
	}

	void startup() override
	{
		ck(FMOD::Studio::System::create(&mSystem), "System::create");
		ck(mSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
		   "System::initialize");
		std::fprintf(stderr, "[audio_demo] FMOD Studio system initialized.\n");
		ck(mSystem->loadBankFile("resources/banks/Master.bank",
		                         FMOD_STUDIO_LOAD_BANK_NORMAL, &mMasterBank),
		   "loadBankFile Master");
		ck(mSystem->loadBankFile("resources/banks/Master.strings.bank",
		                         FMOD_STUDIO_LOAD_BANK_NORMAL, &mStringsBank),
		   "loadBankFile Strings");
		ck(mSystem->loadBankFile("resources/banks/Music.bank",
		                         FMOD_STUDIO_LOAD_BANK_NORMAL, &mMusicBank),
		   "loadBankFile Music");
		std::fprintf(stderr, "[audio_demo] Banks loaded (Master + Strings + Music).\n");

		// 우선 하드코드 이벤트 시도 — 실패 시 Music.bank 안의 첫 이벤트로 fallback.
		FMOD_RESULT r = mSystem->getEvent("event:/Music/Level 01", &mEventDesc);
		if (r != FMOD_OK)
		{
			std::fprintf(stderr, "[audio_demo] event:/Music/Level 01 미존재, fallback...\n");
			int count = 0;
			ck(mMusicBank->getEventCount(&count), "Bank::getEventCount");
			if (count == 0)
			{
				std::fprintf(stderr, "[FMOD] Music.bank 에 이벤트가 없음.\n");
				std::exit(1);
			}
			std::vector<FMOD::Studio::EventDescription *> events(static_cast<size_t>(count));
			ck(mMusicBank->getEventList(events.data(), count, nullptr), "Bank::getEventList");
			mEventDesc = events[0];
		}

		ck(mEventDesc->createInstance(&mInstance), "EventDescription::createInstance");
		ck(mInstance->start(), "EventInstance::start");
		std::fprintf(stderr, "[audio_demo] Event playing.\n");

		int paramCount = 0;
		ck(mEventDesc->getParameterDescriptionCount(&paramCount),
		   "getParameterDescriptionCount");
		for (int i = 0; i < paramCount; ++i)
		{
			FMOD_STUDIO_PARAMETER_DESCRIPTION desc{};
			ck(mEventDesc->getParameterDescriptionByIndex(i, &desc),
			   "getParameterDescriptionByIndex");

			ParamCache p;
			p.name = desc.name;
			p.id = desc.id;
			p.minValue = desc.minimum;
			p.maxValue = desc.maximum;
			p.currentValue = desc.defaultvalue;

			const bool labeled = (desc.flags & FMOD_STUDIO_PARAMETER_LABELED) != 0;
			const bool discrete = (desc.flags & FMOD_STUDIO_PARAMETER_DISCRETE) != 0;

			if (labeled)
			{
				p.kind = ParamCache::Labeled;
				for (int v = static_cast<int>(desc.minimum); v <= static_cast<int>(desc.maximum); ++v)
				{
					char buf[128]{};
					int retrieved = 0;
					mEventDesc->getParameterLabelByID(desc.id, v, buf, sizeof(buf), &retrieved);
					p.labels.emplace_back(buf);
				}
			}
			else if (discrete)
			{
				p.kind = (desc.minimum == 0.0f && desc.maximum == 1.0f)
				             ? ParamCache::Switch
				             : ParamCache::DiscreteInt;
			}
			else
			{
				p.kind = ParamCache::Continuous;
			}

			const char *kindStr = "?";
			switch (p.kind)
			{
			case ParamCache::Continuous:
				kindStr = "Continuous";
				break;
			case ParamCache::Labeled:
				kindStr = "Labeled";
				break;
			case ParamCache::Switch:
				kindStr = "Switch";
				break;
			case ParamCache::DiscreteInt:
				kindStr = "DiscreteInt";
				break;
			}
			std::fprintf(stderr, "[audio_demo] Param[%d] '%s' [%s] %.2f..%.2f default=%.2f\n",
			             i, p.name.c_str(), kindStr,
			             p.minValue, p.maxValue, p.currentValue);

			mParams.push_back(std::move(p));
		}
		std::fprintf(stderr, "[audio_demo] %d parameters discovered.\n", paramCount);

		ck(mSystem->getBus("bus:/", &mMasterBus), "System::getBus root");

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
		ImGui::Begin("Audio Demo");
		ImGui::Text("Parameters (%zu discovered)", mParams.size());
		ImGui::Separator();

		for (auto &p : mParams)
		{
			float v = p.currentValue;
			bool changed = false;
			switch (p.kind)
			{
			case ParamCache::Continuous:
				changed = ImGui::SliderFloat(p.name.c_str(), &v, p.minValue, p.maxValue);
				break;
			case ParamCache::Labeled: {
				int idx = static_cast<int>(v);
				// labels 를 const char* 배열로 변환 (v1.53 Combo 시그니처 호환)
				std::vector<const char *> labelPtrs;
				labelPtrs.reserve(p.labels.size());
				for (auto &s : p.labels)
					labelPtrs.push_back(s.c_str());
				changed = ImGui::Combo(p.name.c_str(), &idx,
				                       labelPtrs.data(), static_cast<int>(labelPtrs.size()));
				v = static_cast<float>(idx);
				break;
			}
			case ParamCache::Switch: {
				bool b = v >= 0.5f;
				changed = ImGui::Checkbox(p.name.c_str(), &b);
				v = b ? 1.0f : 0.0f;
				break;
			}
			case ParamCache::DiscreteInt: {
				int iv = static_cast<int>(v);
				changed = ImGui::SliderInt(p.name.c_str(),
				                           &iv,
				                           static_cast<int>(p.minValue),
				                           static_cast<int>(p.maxValue));
				v = static_cast<float>(iv);
				break;
			}
			}
			if (changed)
			{
				ck(mInstance->setParameterByID(p.id, v), "setParameterByID");
				p.currentValue = v;
			}
		}

		ImGui::Separator();
		ImGui::Text("Transport");
		if (ImGui::Button("Play"))
		{
			ck(mInstance->start(), "EventInstance::start");
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
		{
			ck(mInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT), "EventInstance::stop");
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Pause", &mPaused))
		{
			ck(mInstance->setPaused(mPaused), "EventInstance::setPaused");
		}

		ImGui::Separator();
		if (ImGui::SliderFloat("Master Volume", &mMasterVolume, 0.0f, 1.0f, "%.2f"))
		{
			ck(mMasterBus->setVolume(mMasterVolume), "Bus::setVolume");
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

		if (mInstance)
		{
			mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
			mInstance->release();
			mInstance = nullptr;
			mEventDesc = nullptr;
		}

		if (mMusicBank)
		{
			mMusicBank->unload();
			mMusicBank = nullptr;
		}
		if (mStringsBank)
		{
			mStringsBank->unload();
			mStringsBank = nullptr;
		}
		if (mMasterBank)
		{
			mMasterBank->unload();
			mMasterBank = nullptr;
		}

		if (mSystem)
		{
			ck(mSystem->release(), "System::release");
			mSystem = nullptr;
		}
	}
};

DECLARE_MAIN(audio_demo_application);
