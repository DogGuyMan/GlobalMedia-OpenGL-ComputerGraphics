#ifndef __CHAPTER_2_ENTRY_H__
#define __CHAPTER_2_ENTRY_H__

#include <GL/glcorearb.h>
#include <sb7.h>

namespace SJH::Chapter2
{
	class my_application_1 : public sb7::application
	{
	protected:
		GLfloat colorVectors[4] = {
		    0,
		};

	public:
		virtual void render(double currentTime) override;
	};

	class my_application_2 : public sb7::application
	{
	protected:
		GLfloat colorVectors[4] = {
		    0,
		};

		GLuint rendering_program;
		GLuint vertex_array_object;

		// 셰이더 2개를 프로그램에 링크하고, 셰이더는 삭제 후 프로그램 반환
		GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

	public:
		virtual GLuint compile_vertex_shader();
		virtual GLuint compile_fragment_shader();
		virtual void startup() override;
		virtual void render(double currentTime) override;
		virtual void shutdown() override;
	};

	class my_application_3 : public sb7::application
	{
	protected:
		GLfloat colorVectors[4] = {
		    0,
		};

		GLuint rendering_program;
		GLuint vertex_array_object;

		// 셰이더 2개를 프로그램에 링크하고, 셰이더는 삭제 후 프로그램 반환
		GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

	public:
		virtual GLuint compile_vertex_shader();
		virtual GLuint compile_fragment_shader();
		virtual void startup() override;
		virtual void shutdown() override;
		virtual void render(double currentTime) override;
	};

	class my_application_4 : public sb7::application
	{
	protected:
		GLfloat colorVectors[4] = {
		    0,
		};

		GLuint rendering_program;
		GLuint vertex_array_object;

		GLint loc_time;

		// 셰이더 2개를 프로그램에 링크하고, 셰이더는 삭제 후 프로그램 반환
		GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

	public:
		virtual GLuint compile_vertex_shader();
		virtual GLuint compile_fragment_shader();
		virtual void startup() override;
		virtual void shutdown() override;
		virtual void render(double currentTime) override;
	};
};

#endif //__CHAPTER_2_ENTRY_H__
