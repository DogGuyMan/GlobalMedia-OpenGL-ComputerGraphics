#include <sb7.h>
#include <vmath.h>

// sb7::application 상속받음
// class에서 상속 시 접근 지정자를 생략하면 기본값은 private 상속입니다.
// 이거 까먹은지 너무 오래되었는데 무조건 public 상속을 하자.
class my_application : public sb7::application
{
private:
	GLfloat colorVectors[4] = {0,};
public:
	virtual void render(double currentTime)
	{
		colorVectors[0] = (GLfloat)sin(currentTime)*0.5f + 0.5f;
		colorVectors[1] = (GLfloat)cos(currentTime)*0.5f + 0.5f;
		colorVectors[2] = (GLfloat)sin(currentTime)*0.5f + 0.5f;
		colorVectors[3] = (GLfloat)1.0f;
		glClearBufferfv(GL_COLOR, 0, colorVectors);
	}
};

DECLARE_MAIN(my_application)

/*
// 일명 메인 엔트리 포인트 지정을 한것이다.
int main(int argc, const char **argv) {
	my_application *app = new my_application;
	my_application->run(app);
	delete app;
	return 0;
}
*/