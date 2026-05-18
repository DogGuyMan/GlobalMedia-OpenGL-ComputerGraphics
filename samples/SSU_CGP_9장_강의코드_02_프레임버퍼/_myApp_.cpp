// sb6.h 헤더 파일을 포함시킨다.
#include <sb7.h>
#include <vmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// sb6::application을 상속받는다.
class my_application : public sb7::application
{
public:
	// 쉐이더 컴파일한다.
	GLuint compile_shader1(void)
	{
		GLuint vertex_shader;
		GLuint fragment_shader;
		GLuint program;

		// 버텍스 쉐이더 소스 코드
		static const GLchar * vertex_shader_source[] =
		{
			"#version 430 core											\n"
			"															\n"
			"layout(location = 0) in vec3 pos;							\n"
			"layout(location = 1) in vec3 color;						\n"
			"layout(location = 2) in vec2 texCoord;						\n"
			"															\n"
			"uniform mat4 transform;									\n"
			"															\n"
			"out vec3 vsColor;											\n"
			"out vec2 vsTexCoord;										\n"
			"															\n"
			"void main(void)											\n"
			"{															\n"
			"	gl_Position = transform*vec4(pos.x, pos.y, pos.z, 1.0);	\n"
			"															\n"
			"	vsColor = color;										\n"
			"	vsTexCoord = texCoord;									\n"
			"}															\n"
		};

		// 프래그먼트 쉐이더 소스 코드
		static const GLchar * fragment_shader_source[] =
		{
			"#version 430 core								\n"
			"												\n"
			"in vec3 vsColor;								\n"
			"in vec2 vsTexCoord;							\n"
			"uniform sampler2D texture1;					\n"
			"												\n"
			"out vec4 fragColor;							\n"
			"												\n"
			"void main(void)								\n"
			"{												\n"
			"	fragColor = texture(texture1, vsTexCoord);	\n"
			"}												\n"
		};

		// 버텍스 쉐이더를 생성하고 컴파일한다.
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 1, vertex_shader_source, NULL);
		glCompileShader(vertex_shader);

		// 프래그먼트 쉐이더를 생성하고 컴파일한다.
		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, fragment_shader_source, NULL);
		glCompileShader(fragment_shader);

		// 프로그램을 생성하고 쉐이더를 Attach시키고 링크한다.
		program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		// 이제 프로그램이 쉐이더를 소유하므로 쉐이더를 삭제한다.
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		return program;
	}

	GLuint compile_shader2(void)
	{
		GLuint vertex_shader;
		GLuint fragment_shader;
		GLuint program;

		// 버텍스 쉐이더 소스 코드
		static const GLchar * vertex_shader_source[] =
		{
			"#version 430 core											\n"
			"															\n"
			"layout(location = 0) in vec3 pos;							\n"
			"layout(location = 1) in vec3 color;						\n"
			"															\n"
			"uniform mat4 transform;									\n"
			"															\n"
			"out vec3 vsColor;											\n"
			"															\n"
			"void main(void)											\n"
			"{															\n"
			"	gl_Position = transform*vec4(pos.x, pos.y, pos.z, 1.0);	\n"
			"															\n"
			"	vsColor = color;										\n"
			"}															\n"
		};

		// 프래그먼트 쉐이더 소스 코드
		static const GLchar * fragment_shader_source[] =
		{
			"#version 430 core						\n"
			"in vec3 vsColor;						\n"
			"out vec4 fragColor;					\n"
			"										\n"
			"void main(void)						\n"
			"{										\n"
			"	fragColor = vec4(vsColor, 1.0);		\n"
			"}										\n"
		};

		// 버텍스 쉐이더를 생성하고 컴파일한다.
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 1, vertex_shader_source, NULL);
		glCompileShader(vertex_shader);

		// 프래그먼트 쉐이더를 생성하고 컴파일한다.
		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, fragment_shader_source, NULL);
		glCompileShader(fragment_shader);

		// 프로그램을 생성하고 쉐이더를 Attach시키고 링크한다.
		program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		// 이제 프로그램이 쉐이더를 소유하므로 쉐이더를 삭제한다.
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		return program;
	}

	// 애플리케이션 초기화 수행한다.
	virtual void startup()
	{
		// 쉐이더 프로그램 컴파일 및 연결
		shader_programs[0] = compile_shader1();
		shader_programs[1] = compile_shader2();

		// VAO, VBO, EBO, texture 생성
		glGenVertexArrays(3, VAOs);
		glGenBuffers(3, VBOs);
		glGenBuffers(2, EBOs);
		glGenTextures(2, textures);

		// 첫 번째 객체 정의 : 바닥 --------------------------------------------------
		glBindVertexArray(VAOs[0]);
		// 바닥 점들의 위치와 컬러, 텍스처 좌표를 정의한다.
		float floor_s = 3.0f, floor_t = 3.0f;
		GLfloat floor_vertices[] = {
			1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, floor_s, floor_t,  // 우측 상단
			-1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, floor_t,  // 좌측 상단
			-1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   // 좌측 하단
			1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, floor_s, 0.0f   // 우측 하단
		};

		// 삼각형으로 그릴 인덱스를 정의한다.
		GLuint floor_indices[] = {
			0, 1, 2,	// 첫번째 삼각형
			0, 2, 3		// 두번째 삼각형
		};

		// VBO를 생성하여 vertices 값들을 복사
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(floor_vertices), floor_vertices, GL_STATIC_DRAW);

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// 컬러 속성 (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// 텍스처 좌표 속성 (location = 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		// EBO를 생성하고 indices 값들을 복사
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[0]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floor_indices), floor_indices, GL_STATIC_DRAW);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// 텍스처 객체 만들고 바인딩			
		glBindTexture(GL_TEXTURE_2D, textures[0]);

		// 텍스처 이미지 로드하기
		int width, height, nrChannels;
		unsigned char *data = stbi_load("wall.jpg", &width, &height, &nrChannels, 0);

		if (data) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		stbi_image_free(data);

		// 텍스처 샘플링/필터링 설정
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// 두 번째 객체 정의 : 박스 --------------------------------------------------
		glBindVertexArray(VAOs[1]);
		// 박스 점들의 위치와 컬러, 텍스처 좌표를 정의한다.
		float box_s = 1.0f, box_t = 1.0f;
		GLfloat box_vertices[] = {
			// 뒷면
			-0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f, box_s, box_t,
			0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			-0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 0.0f, box_s, 0.0f,

			0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			-0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f, box_s, box_t,
			0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f, box_t,
			// 우측면
			0.25f, 0.0f, -0.25f, 0.0f, 1.0f, 0.0f, box_s, 0.0f,
			0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 0.0f, box_s, box_t,
			0.25f, 0.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,

			0.25f, 0.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 0.0f, box_s, box_t,
			0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, box_t,
			// 정면
			0.25f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, box_s, 0.0f,
			0.25f, 0.5f, 0.25f, 0.0f, 0.0f, 1.0f, box_s, box_t,
			-0.25f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,

			-0.25f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.25f, 0.5f, 0.25f, 0.0f, 0.0f, 1.0f, box_s, box_t,
			-0.25f, 0.5f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, box_t,
			// 좌측면
			-0.25f, 0.0f, 0.25f, 1.0f, 0.0f, 1.0f, box_s, 0.0f,
			-0.25f, 0.5f, 0.25f, 1.0f, 0.0f, 1.0f, box_s, box_t,
			-0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,

			-0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			-0.25f, 0.5f, 0.25f, 1.0f, 0.0f, 1.0f, box_s, box_t,
			-0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 1.0f, 0.0f, box_t,
			// 바닥면
			-0.25f, 0.0f, 0.25f, 1.0f, 1.0f, 0.0f, box_s, 0.0f,
			0.25f, 0.0f, -0.25f, 1.0f, 1.0f, 0.0f, 0.0f, box_t,
			0.25f, 0.0f, 0.25f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,

			0.25f, 0.0f, -0.25f, 1.0f, 1.0f, 0.0f, 0.0f, box_t,
			-0.25f, 0.0f, 0.25f, 1.0f, 1.0f, 0.0f, box_s, 0.0,
			-0.25f, 0.0f, -0.25f, 1.0f, 1.0f, 0.0f, box_s, box_t,
			// 윗면
			-0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 1.0f, 0.0f, box_t,
			0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 1.0f, box_s, 0.0f,
			0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 1.0f, box_s, box_t,

			0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 1.0f, box_s, 0.0f,
			-0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 1.0f, 0.0f, box_t,
			-0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f
		};

		// VBO를 생성하여 vertices 값들을 복사
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// 컬러 속성 (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// 텍스처 좌표 속성 (location = 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// 텍스처 객체 만들고 바인딩			
		glBindTexture(GL_TEXTURE_2D, textures[1]);

		// 텍스처 이미지 로드하기
		width, height, nrChannels;
		data = stbi_load("container.jpg", &width, &height, &nrChannels, 0);

		if (data) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		stbi_image_free(data);

		// 텍스처 샘플링/필터링 설정
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		//  세 번째 객체 정의 : 피라미드 --------------------------------------------------
		glBindVertexArray(VAOs[2]);
		// 피라미드 점들의 위치와 컬러, 텍스처 좌표를 정의한다.
		GLfloat pyramid_vertices[] = {
			1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f,   // 우측 상단
			-1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f,  // 좌측 상단
			-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,   // 좌측 하단
			1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,    // 우측 하단
			0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f      // 꼭지점
		};

		// 삼각형으로 그릴 인덱스를 정의한다.
		GLuint pyramid_indices[] = {
			0, 2, 1,
			0, 3, 2,
			4, 0, 1,
			4, 1, 2,
			4, 2, 3,
			4, 3, 0
		};

		// VBO를 생성하여 vertices 값들을 복사
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(pyramid_vertices), pyramid_vertices, GL_STATIC_DRAW);

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// 컬러 속성 (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		// EBO를 생성하고 indices 값들을 복사
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(pyramid_indices), pyramid_indices, GL_STATIC_DRAW);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// framebuffer 설정 --------------------------------------------------
		glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		// color buffer 텍스처 생성 및 연결
		glGenTextures(1, &FBO_texture);
		glBindTexture(GL_TEXTURE_2D, FBO_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, info.windowWidth, info.windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBO_texture, 0);
		// depth&stencil buffer를 위한 Render Buffer Object 생성 및 연결
		GLuint RBO;
		glGenRenderbuffers(1, &RBO);
		glBindRenderbuffer(GL_RENDERBUFFER, RBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, info.windowWidth, info.windowHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO); 
		// Framebuffer가 정상적으로 만들어 졌는지 확인
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			glfwTerminate();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}

	// 애플리케이션 끝날 때 호출된다.
	virtual void shutdown()
	{
		glDeleteTextures(2, textures);
		glDeleteBuffers(2, EBOs);
		glDeleteBuffers(3, VBOs);
		glDeleteVertexArrays(3, VAOs);
		glDeleteProgram(shader_programs[0]);
		glDeleteProgram(shader_programs[1]);
	}

	// 렌더링 virtual 함수를 작성해서 오버라이딩한다.
	virtual void render(double currentTime)
	{
		glEnable(GL_CULL_FACE);
		glViewport(0, 0, info.windowWidth, info.windowHeight);

		GLint uniform_transform1 = glGetUniformLocation(shader_programs[0], "transform");
		GLint uniform_transform2 = glGetUniformLocation(shader_programs[1], "transform");

		// 카메라 매트릭스 계산
		float distance = 2.f;
		vmath::vec3 eye((float)cos(currentTime*0.1f)*distance, 1.0, (float)sin(currentTime*0.1f)*distance);
		vmath::vec3 center(0.0, 0.0, 0.0);
		vmath::vec3 up(0.0, 1.0, 0.0);
		vmath::mat4 lookAt = vmath::lookat(eye, center, up);
		float fov = 50.f;// (float)cos(currentTime)*20.f + 50.0f;
		vmath::mat4 projM = vmath::perspective(fov, (float)info.windowWidth / (float)info.windowHeight, 0.1f, 1000.0f);

		// Render-To-Texture Framebuffer 바인딩 ----------------------------------------
		// FBO 바인딩
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glEnable(GL_DEPTH_TEST); 
		// FBO에 연결된 버퍼들의 값을 지우고, 뎁스 테스팅 활성화
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 박스 그리기 ---------------------------------------
		vmath::mat4 transM = vmath::translate(vmath::vec3((float)sin(currentTime), 0.0f, (float)cos(currentTime) * 0.7f));
		float angle = currentTime * 150;
		vmath::mat4 rotateM = vmath::rotate(angle, 0.0f, 1.0f, 0.0f);

		glUseProgram(shader_programs[0]);
		glUniformMatrix4fv(uniform_transform1, 1, GL_FALSE, projM*lookAt*transM*rotateM);
		glUniform1i(glGetUniformLocation(shader_programs[0], "texture1"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[1]);
		glBindVertexArray(VAOs[1]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		
		// 바닥 그리기 ---------------------------------------
		glUseProgram(shader_programs[0]);
		glUniformMatrix4fv(uniform_transform1, 1, GL_FALSE, projM*lookAt*vmath::scale(1.5f));
		glUniform1i(glGetUniformLocation(shader_programs[0], "texture1"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[0]);
		glBindVertexArray(VAOs[0]);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// 피라미드 그리기 ---------------------------------------
		float move_y = (float)cos(currentTime)*0.2f + 0.5f;
		float scaleFactor = (float)cos(currentTime)*0.05f + 0.2f;
		vmath::mat4 transform = vmath::translate(0.0f, move_y, 0.0f)*
			vmath::rotate(angle*0.5f, 0.0f, 1.0f, 0.0f)*
			vmath::scale(scaleFactor, scaleFactor, scaleFactor);

		glUseProgram(shader_programs[1]);
		glUniformMatrix4fv(uniform_transform2, 1, GL_FALSE, projM*lookAt*transform);
		glBindVertexArray(VAOs[2]);
		glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_INT, 0);

		// 윈도우 Framebuffer 바인딩 ----------------------------------------
		// 기본 Framebuffer로 되돌리기
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_DEPTH_TEST); 
		// 버퍼들의 값 지우기
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f); 
		glClear(GL_COLOR_BUFFER_BIT);
		// FBO Texture를 쉐이더 프로그램에 연결
		glUseProgram(shader_programs[0]);
		glUniformMatrix4fv(uniform_transform1, 1, GL_FALSE, projM*lookAt*rotateM*vmath::scale(1.3f)*vmath::translate(0.0f, -0.25f, 0.0f));
		glUniform1i(glGetUniformLocation(shader_programs[0], "texture1"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, FBO_texture);
		glBindVertexArray(VAOs[1]);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	void drawScene(double currentTime)
	{

	}

	void onResize(int w, int h)
	{
		sb7::application::onResize(w, h);
	}

private:
	GLuint shader_programs[2];
	GLuint VAOs[3], VBOs[3], EBOs[2];
	GLuint textures[2];
	GLuint FBO, FBO_texture;
};

// DECLARE_MAIN의 하나뿐인 인스턴스
DECLARE_MAIN(my_application)