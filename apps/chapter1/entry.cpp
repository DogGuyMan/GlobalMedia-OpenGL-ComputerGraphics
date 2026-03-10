#include "entry.h"
#include <vmath.h>

// sb7::application 상속받음
// class에서 상속 시 접근 지정자를 생략하면 기본값은 private 상속입니다.
// 이거 까먹은지 너무 오래되었는데 무조건 public 상속을 하자.
namespace SJH::Chapter1
{
	void my_application::render(double currentTime)
	{
		static const GLfloat red[] = {1.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 0, red);
	}
};