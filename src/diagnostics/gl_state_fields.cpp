#include "diagnostics/gl_state_fields.h"

#include "log_util.h"

#include <cstdio>

namespace SJH::Diagnostics
{
    const char* SymbolicName(GLenum e)
    {
        // GL_TEXTURE0..GL_TEXTURE15 동적 영역
        if (e >= GL_TEXTURE0 && e <= GL_TEXTURE0 + 15) {
            // thread_local 정적 버퍼 — caller가 즉시 출력하면 안전, 보관 시엔 std::string 권장
            static thread_local char buf[16];
            snprintf(buf, sizeof(buf), "GL_TEXTURE%d", static_cast<int>(e - GL_TEXTURE0));
            return buf;
        }

        switch (e) {
            // depth_func / stencil_func 공용 8개
            case GL_NEVER:    return "GL_NEVER";
            case GL_LESS:     return "GL_LESS";
            case GL_EQUAL:    return "GL_EQUAL";
            case GL_LEQUAL:   return "GL_LEQUAL";
            case GL_GREATER:  return "GL_GREATER";
            case GL_NOTEQUAL: return "GL_NOTEQUAL";
            case GL_GEQUAL:   return "GL_GEQUAL";
            case GL_ALWAYS:   return "GL_ALWAYS";

            // blend factor — GL 3.3 core 한정 (SRC1 family는 4.4+ 제외)
            // 0은 GL_ZERO — blend factor 컨텍스트 가정 (spec 2.1)
            case 0:                              return "GL_ZERO";
            case GL_ONE:                         return "GL_ONE";
            case GL_SRC_COLOR:                   return "GL_SRC_COLOR";
            case GL_ONE_MINUS_SRC_COLOR:         return "GL_ONE_MINUS_SRC_COLOR";
            case GL_DST_COLOR:                   return "GL_DST_COLOR";
            case GL_ONE_MINUS_DST_COLOR:         return "GL_ONE_MINUS_DST_COLOR";
            case GL_SRC_ALPHA:                   return "GL_SRC_ALPHA";
            case GL_ONE_MINUS_SRC_ALPHA:         return "GL_ONE_MINUS_SRC_ALPHA";
            case GL_DST_ALPHA:                   return "GL_DST_ALPHA";
            case GL_ONE_MINUS_DST_ALPHA:         return "GL_ONE_MINUS_DST_ALPHA";
            case GL_CONSTANT_COLOR:              return "GL_CONSTANT_COLOR";
            case GL_ONE_MINUS_CONSTANT_COLOR:    return "GL_ONE_MINUS_CONSTANT_COLOR";
            case GL_CONSTANT_ALPHA:              return "GL_CONSTANT_ALPHA";
            case GL_ONE_MINUS_CONSTANT_ALPHA:    return "GL_ONE_MINUS_CONSTANT_ALPHA";
            case GL_SRC_ALPHA_SATURATE:          return "GL_SRC_ALPHA_SATURATE";

            // cull/front face
            case GL_FRONT:           return "GL_FRONT";
            case GL_BACK:            return "GL_BACK";
            case GL_FRONT_AND_BACK:  return "GL_FRONT_AND_BACK";
            case GL_CCW:             return "GL_CCW";
            case GL_CW:              return "GL_CW";

            // vertex attribute types (audit 트랙 A) — GL 3.3 core
            case GL_BYTE:            return "GL_BYTE";
            case GL_UNSIGNED_BYTE:   return "GL_UNSIGNED_BYTE";
            case GL_SHORT:           return "GL_SHORT";
            case GL_UNSIGNED_SHORT:  return "GL_UNSIGNED_SHORT";
            case GL_INT:             return "GL_INT";
            case GL_UNSIGNED_INT:    return "GL_UNSIGNED_INT";
            case GL_FLOAT:           return "GL_FLOAT";
            case GL_DOUBLE:          return "GL_DOUBLE";
            case GL_HALF_FLOAT:      return "GL_HALF_FLOAT";
        }

        // 미적중 — hex fallback. 4자리 대문자 (예: "0xDEAD")
        // thread_local 버퍼: caller가 즉시 출력 가정. 영구 보관 시 fmt::format 사용 권장.
        static thread_local char buf[16];
        snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(e));
        return buf;
    }

    /// GLStateFields → 사람이 읽는 다중라인. Task 5 본 구현 + audit 트랙 A
    /// (attribute_layouts 출력) 통합.
    ///
    /// 포맷 정책:
    /// - enum 필드: SymbolicName (예: "GL_LESS")
    /// - GLuint 핸들: raw 정수 (의도된 비대칭 — handle은 의미 없는 식별자)
    /// - VAO=0이면 element_buffer 라인에 주석 ("EBO state is per-VAO; ...")
    /// - texture_2d_per_unit / attribute_layouts: 0이 아닌 / enabled인 항목만 출력 (노이즈 최소화)
    std::string FieldsToString(const GLStateFields& f)
    {
        std::string out;
        out.reserve(512);

        // 헤더 — 비대칭 (enum=symbolic, handle=raw)을 한 줄 설명
        out += "# GL state (enum=symbolic, handle=raw integer)\n";

        out += Log::Format("vao:            {}\n", f.vao);
        out += Log::Format("program:        {}\n", f.program);
        out += Log::Format("array_buffer:   {}\n", f.array_buffer);

        // VAO=0 일 때 element_buffer 라인에 주석 (spec 4.4 — 사용자 EBO incident memory 반영)
        if (f.vao == 0) {
            out += Log::Format("element_buffer: {}  (note: EBO state is per-VAO; with VAO=0, this is always 0)\n",
                               f.element_buffer);
        } else {
            out += Log::Format("element_buffer: {}\n", f.element_buffer);
        }

        out += Log::Format("draw_fbo:       {}\n", f.draw_fbo);
        out += Log::Format("read_fbo:       {}\n", f.read_fbo);
        out += Log::Format("active_texture: {}\n", SymbolicName(f.active_texture));

        // 텍스처 unit — 0이 아닌 것만 출력 (노이즈 최소화)
        bool any_unit = false;
        for (int i = 0; i < 16; ++i) {
            if (f.texture_2d_per_unit[i] != 0) {
                out += Log::Format("tex_2d[unit {}]: {}\n", i, f.texture_2d_per_unit[i]);
                any_unit = true;
            }
        }
        if (!any_unit) {
            out += "tex_2d[*]:      (all units empty)\n";
        }

        out += Log::Format("viewport:       [{}, {}, {}, {}]\n",
                           f.viewport[0], f.viewport[1], f.viewport[2], f.viewport[3]);

        out += Log::Format("depth_test:     {}\n", f.depth_test_enabled ? "ENABLED" : "disabled");
        out += Log::Format("depth_func:     {}\n", SymbolicName(f.depth_func));
        out += Log::Format("depth_write:    {}\n", f.depth_write_mask ? "true" : "false");

        out += Log::Format("blend:          {}\n", f.blend_enabled ? "ENABLED" : "disabled");
        out += Log::Format("blend_src_rgb:  {}\n", SymbolicName(f.blend_src_rgb));
        out += Log::Format("blend_dst_rgb:  {}\n", SymbolicName(f.blend_dst_rgb));

        out += Log::Format("cull_face:      {}\n", f.cull_face_enabled ? "ENABLED" : "disabled");
        out += Log::Format("cull_face_mode: {}\n", SymbolicName(f.cull_face_mode));
        out += Log::Format("front_face:     {}\n", SymbolicName(f.front_face));

        out += Log::Format("color_write:    [{}, {}, {}, {}]\n",
                           f.color_write_mask[0] ? 'R':'-',
                           f.color_write_mask[1] ? 'G':'-',
                           f.color_write_mask[2] ? 'B':'-',
                           f.color_write_mask[3] ? 'A':'-');

        out += Log::Format("clear_color:    [{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                           f.clear_color[0], f.clear_color[1], f.clear_color[2], f.clear_color[3]);

        // attribute_layouts (audit 트랙 A) — enabled인 slot만 출력
        bool any_attrib = false;
        for (size_t i = 0; i < f.attribute_layouts.size(); ++i) {
            const auto& a = f.attribute_layouts[i];
            if (a.enabled) {
                out += Log::Format("attrib[{}]:      vec{} {}, normalized={}, stride={}, vbo={}\n",
                                   i, a.size, SymbolicName(a.type),
                                   a.normalized ? "true" : "false",
                                   a.stride, a.buffer_binding);
                any_attrib = true;
            }
        }
        if (!any_attrib) {
            out += "attrib[*]:      (all disabled)\n";
        }

        return out;
    }

    namespace
    {
        /// 단일 attribute slot의 layout을 query.
        /// glGetVertexAttribiv는 부수효과 0 — rebind 불필요.
        VertexAttribInfo QueryAttribInfo(GLuint index)
        {
            VertexAttribInfo info;
            GLint tmp = 0;

            glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &tmp);
            info.enabled = (tmp == GL_TRUE);

            glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &tmp);
            info.size = tmp;

            glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &tmp);
            info.type = static_cast<GLenum>(tmp);

            glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &tmp);
            info.normalized = (tmp == GL_TRUE);

            glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &tmp);
            info.stride = static_cast<GLsizei>(tmp);

            glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &tmp);
            info.buffer_binding = static_cast<GLuint>(tmp);

            return info;
        }
    }

    /// 현재 GL 상태 캡처. Task 2 Step 4 본 구현 + audit 트랙 A 확장 (vertex attribute layouts).
    GLStateFields CaptureGLState()
    {
        // 1. drain pre-existing errors — 캡처 자체의 에러를 후속 post-check에서만 잡도록
        while (glGetError() != GL_NO_ERROR) {}

        GLStateFields f;

        // 2. 단일값 GLuint 바인딩
        GLint tmp = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,         &tmp); f.vao            = static_cast<GLuint>(tmp);
        glGetIntegerv(GL_CURRENT_PROGRAM,              &tmp); f.program        = static_cast<GLuint>(tmp);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,         &tmp); f.array_buffer   = static_cast<GLuint>(tmp);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &tmp); f.element_buffer = static_cast<GLuint>(tmp);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,     &tmp); f.draw_fbo       = static_cast<GLuint>(tmp);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,     &tmp); f.read_fbo       = static_cast<GLuint>(tmp);

        // 3. active_texture (GLenum)
        glGetIntegerv(GL_ACTIVE_TEXTURE, &tmp);
        f.active_texture = static_cast<GLenum>(tmp);

        // 4. viewport (4 ints)
        glGetIntegerv(GL_VIEWPORT, f.viewport.data());

        // 5. 픽셀 파이프라인 — glIsEnabled / glGetIntegerv / glGetBooleanv 혼합
        f.depth_test_enabled = (glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
        glGetIntegerv(GL_DEPTH_FUNC, &tmp); f.depth_func = static_cast<GLenum>(tmp);
        GLboolean b = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &b); f.depth_write_mask = (b == GL_TRUE);

        f.blend_enabled = (glIsEnabled(GL_BLEND) == GL_TRUE);
        glGetIntegerv(GL_BLEND_SRC_RGB, &tmp); f.blend_src_rgb = static_cast<GLenum>(tmp);
        glGetIntegerv(GL_BLEND_DST_RGB, &tmp); f.blend_dst_rgb = static_cast<GLenum>(tmp);

        f.cull_face_enabled = (glIsEnabled(GL_CULL_FACE) == GL_TRUE);
        glGetIntegerv(GL_CULL_FACE_MODE, &tmp); f.cull_face_mode = static_cast<GLenum>(tmp);
        glGetIntegerv(GL_FRONT_FACE,     &tmp); f.front_face     = static_cast<GLenum>(tmp);

        GLboolean cwm[4] = {};
        glGetBooleanv(GL_COLOR_WRITEMASK, cwm);
        for (int i = 0; i < 4; ++i) f.color_write_mask[i] = (cwm[i] == GL_TRUE);

        glGetFloatv(GL_COLOR_CLEAR_VALUE, f.clear_color.data());

        // 6. 텍스처 unit 16개 — active_texture 보존/복원
        GLint saved_active = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_active);
        for (int i = 0; i < 16; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &tmp);
            f.texture_2d_per_unit[i] = static_cast<GLuint>(tmp);
        }
        glActiveTexture(static_cast<GLenum>(saved_active));

        // 7. Vertex attribute layouts — audit 트랙 A 추가
        // glGetVertexAttribiv는 부수효과 0; 현재 바인딩된 VAO의 attribute state를 query
        for (GLuint i = 0; i < 16; ++i) {
            f.attribute_layouts[i] = QueryAttribInfo(i);
        }

        // 8. post-check
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            Log::Warn("[GLStateLog::Capture] produced GL error 0x{:X} — "
                         "all fields populated but values may be stale",
                         static_cast<unsigned>(err));
        }

        return f;
    }
}
