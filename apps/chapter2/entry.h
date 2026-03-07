#ifndef __CHAPTER_2_ENTRY_H__
#define __CHAPTER_2_ENTRY_H__

#include <GL/glcorearb.h>
#include <sb7.h>

namespace SJH::Chapter2{
	
	class my_application_base :public sb7::application {
protected:
		GLfloat colorVectors[4] = {0,};
	};

	class my_application_1 :public my_application_base {
public:
		virtual void render(double currentTime) override;
	};

	class my_application_2 :public my_application_base {
protected:
	GLuint rendering_program;
	GLuint vertex_array_object;
public:
		virtual GLuint compile_shaders(void);
		virtual void startup() override;
		virtual void shutdown() override;
		virtual void render(double currentTime) override;
	};

	class my_application_3 : public my_application_2 {
		virtual GLuint compile_shaders(void) override;
		virtual void render(double currentTime) override;
	};
};

#endif //__CHAPTER_2_ENTRY_H__
