#include <sb7.h>
#include <optional>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace SJH::exercise1::vertexcase
{
	std::optional<std::string> LoadTextFile(const char *path)
	{
		std::ifstream fin(path);
		if (!fin.is_open())
		{
			fprintf(stderr, "Failed to initialize GLFW\n");
			return {};
		}
		std::stringstream text;
		text << fin.rdbuf();
		return text.str();
	}

	class vc_application_base : public sb7::application
	{
	public:
		virtual void startup();
		virtual void render(float currentTime);
		virtual void shutdown();
	};

	class vc_application_1 : public vc_application_base
	{
	private:
	public:
		virtual void startup() override
		{
		}

		virtual void render(float currentTime) override
		{
		}

		virtual void shutdown() override
		{
		}
	};

};