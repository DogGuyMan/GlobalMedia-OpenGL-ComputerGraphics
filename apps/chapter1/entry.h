#ifndef __CHAPTER_1_ENTRY_H__
#define __CHAPTER_1_ENTRY_H__

#include <GL/glcorearb.h>
#include <sb7.h>

namespace SJH::Chapter1{
	class my_application :public sb7::application {
public:
		virtual void render(double currentTime);
	};
};

#endif //__CHAPTER_1_ENTRY_H__