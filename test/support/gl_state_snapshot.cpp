/**
 * @file gl_state_snapshot.cpp
 * @brief @c GLStateSnapshot::Capture / ToString + @c Diff 구현.
 */

#include "support/gl_state_snapshot.h"

#include <spdlog/fmt/fmt.h>

namespace SJH::test
{
    GLStateSnapshot GLStateSnapshot::Capture()
    {
        return GLStateSnapshot{ SJH::Diagnostics::CaptureGLState() };
    }

    std::string GLStateSnapshot::ToString() const
    {
        return SJH::Diagnostics::FieldsToString(fields);
    }

    namespace
    {
        using SJH::Diagnostics::SymbolicName;
        using SJH::Diagnostics::VertexAttribInfo;

        /// 단일 필드 비교 출력 — 변화 있으면 line 추가.
        template <typename T>
        void DiffField(std::string& out, const char* name, const T& a, const T& b)
        {
            if (a != b) {
                out += fmt::format("  {}: {} -> {}\n", name, a, b);
            }
        }

        /// enum 전용 (SymbolicName 적용) — 비대칭 정책의 enum 쪽
        void DiffEnum(std::string& out, const char* name, GLenum a, GLenum b)
        {
            if (a != b) {
                out += fmt::format("  {}: {} -> {}\n", name, SymbolicName(a), SymbolicName(b));
            }
        }

        /// VertexAttribInfo 변화를 한 줄로. 변화 있으면 enabled / size·type / stride / vbo 모두 출력.
        /// (audit 트랙 A — 카테고리 C 사보타지 가시화)
        void DiffAttrib(std::string& out, size_t slot,
                        const VertexAttribInfo& a, const VertexAttribInfo& b)
        {
            if (a == b) return;

            // before/after 형식: "vec3 GL_FLOAT, stride=24, vbo=2"
            auto descr = [](const VertexAttribInfo& v) {
                if (!v.enabled) return std::string("disabled");
                return fmt::format("vec{} {}, normalized={}, stride={}, vbo={}",
                                   v.size, SymbolicName(v.type),
                                   v.normalized ? "true" : "false",
                                   v.stride, v.buffer_binding);
            };
            out += fmt::format("  attrib[{}]: ({}) -> ({})\n", slot, descr(a), descr(b));
        }
    }

    std::string Diff(const GLStateSnapshot& A, const GLStateSnapshot& B)
    {
        const auto& a = A.fields;
        const auto& b = B.fields;
        std::string out;

        // 핸들 (raw 정수 — 비대칭 정책)
        DiffField(out, "vao",            a.vao,            b.vao);
        DiffField(out, "program",        a.program,        b.program);
        DiffField(out, "array_buffer",   a.array_buffer,   b.array_buffer);

        // element_buffer — VAO=0 시 주석 (변화 있을 때만, 사용자 EBO incident memory 반영)
        if (a.element_buffer != b.element_buffer) {
            const bool either_zero = (a.vao == 0 || b.vao == 0);
            if (either_zero) {
                out += fmt::format("  element_buffer: {} -> {}  "
                                   "(note: EBO state is per-VAO; with VAO=0, this is always 0)\n",
                                   a.element_buffer, b.element_buffer);
            } else {
                out += fmt::format("  element_buffer: {} -> {}\n",
                                   a.element_buffer, b.element_buffer);
            }
        }

        DiffField(out, "draw_fbo",       a.draw_fbo,       b.draw_fbo);
        DiffField(out, "read_fbo",       a.read_fbo,       b.read_fbo);
        DiffEnum (out, "active_texture", a.active_texture, b.active_texture);

        // 텍스처 unit — 변화한 unit만
        for (int i = 0; i < 16; ++i) {
            if (a.texture_2d_per_unit[i] != b.texture_2d_per_unit[i]) {
                out += fmt::format("  tex_2d[unit {}]: {} -> {}\n", i,
                                   a.texture_2d_per_unit[i], b.texture_2d_per_unit[i]);
            }
        }

        // viewport (전체 array가 변하면 한 줄로)
        if (a.viewport != b.viewport) {
            out += fmt::format("  viewport: [{},{},{},{}] -> [{},{},{},{}]\n",
                               a.viewport[0], a.viewport[1], a.viewport[2], a.viewport[3],
                               b.viewport[0], b.viewport[1], b.viewport[2], b.viewport[3]);
        }

        DiffField(out, "depth_test",     a.depth_test_enabled, b.depth_test_enabled);
        DiffEnum (out, "depth_func",     a.depth_func,         b.depth_func);
        DiffField(out, "depth_write",    a.depth_write_mask,   b.depth_write_mask);

        DiffField(out, "blend",          a.blend_enabled, b.blend_enabled);
        DiffEnum (out, "blend_src_rgb",  a.blend_src_rgb, b.blend_src_rgb);
        DiffEnum (out, "blend_dst_rgb",  a.blend_dst_rgb, b.blend_dst_rgb);

        DiffField(out, "cull_face",      a.cull_face_enabled, b.cull_face_enabled);
        DiffEnum (out, "cull_face_mode", a.cull_face_mode,    b.cull_face_mode);
        DiffEnum (out, "front_face",     a.front_face,        b.front_face);

        if (a.color_write_mask != b.color_write_mask) {
            auto fmt4 = [](const std::array<bool, 4>& m) {
                return fmt::format("[{},{},{},{}]",
                                   m[0] ? 'R' : '-',
                                   m[1] ? 'G' : '-',
                                   m[2] ? 'B' : '-',
                                   m[3] ? 'A' : '-');
            };
            out += fmt::format("  color_write: {} -> {}\n",
                               fmt4(a.color_write_mask), fmt4(b.color_write_mask));
        }

        if (a.clear_color != b.clear_color) {
            out += fmt::format("  clear_color: [{:.3f},{:.3f},{:.3f},{:.3f}] -> "
                                              "[{:.3f},{:.3f},{:.3f},{:.3f}]\n",
                               a.clear_color[0], a.clear_color[1], a.clear_color[2], a.clear_color[3],
                               b.clear_color[0], b.clear_color[1], b.clear_color[2], b.clear_color[3]);
        }

        // attribute_layouts (audit 트랙 A) — slot당 변화 출력
        for (size_t i = 0; i < a.attribute_layouts.size(); ++i) {
            DiffAttrib(out, i, a.attribute_layouts[i], b.attribute_layouts[i]);
        }

        if (out.empty()) return "(no GL state change)\n";
        return "GL State Diff:\n" + out;
    }
}
