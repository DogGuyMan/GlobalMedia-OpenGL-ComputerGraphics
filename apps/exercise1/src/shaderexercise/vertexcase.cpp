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
			std::cout << "failed to open file" << std::endl;
			return {};
		}
		std::stringstream text;
		text << fin.rdbuf();
		return text.str();
	}

	class vc_application_base : public sb7::application
	{
	public:
		GLuint CompileShader(const char *path);
		virtual void startup();
		virtual void render(float currentTime);
	};

	class vc_application_base : public vc_application_base
	{
	};

};