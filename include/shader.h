#ifndef __SB7_SHADER_H__
#define __SB7_SHADER_H__

// 가드 이름은 __SB7_SHADER_H__ — SJH::Shader 의 src/shader/shader.h 가 __SJH_SHADER_H__ 사용.
// 둘이 같은 가드 쓰면 한쪽 include 시 다른 쪽 skip 되어 namespace 미정의 빌드 에러.
// sb7 vendored 헤더는 sb7.h / sb7ktx.h / sb7textoverlay.h 의 __SB7_*_H__ 패턴 일관.

namespace sb7
{

namespace shader
{

GLuint load(const char * filename,
            GLenum shader_type = GL_FRAGMENT_SHADER,
#ifdef _DEBUG
            bool check_errors = true);
#else
            bool check_errors = false);
#endif

GLuint from_string(const char * source,
                   GLenum shader_type,
#ifdef _DEBUG
                   bool check_errors = true);
#else
                   bool check_errors = false);
#endif

}

namespace program
{

GLuint link_from_shaders(const GLuint * shaders,
                         int shader_count,
                         bool delete_shaders,
#ifdef _DEBUG
                         bool check_errors = true);
#else
                         bool check_errors = false);
#endif

}

}

#endif /* __SJH_SHADER_H__ */
