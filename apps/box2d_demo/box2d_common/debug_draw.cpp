#include "debug_draw.h"
#include <cstring>
#include <cmath>

static const char* kVertSrc = R"(
#version 410 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uProj;
out vec4 vColor;
void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char* kFragSrc = R"(
#version 410 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)";

static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

// ---- Camera2D ----

void Camera2D::BuildProjectionMatrix(float* m) const
{
    float ratio = float(width) / float(height);
    float extX   = ratio * 25.0f * zoom;
    float extY   = 25.0f * zoom;
    float l = center.x - extX, r = center.x + extX;
    float b = center.y - extY, t = center.y + extY;

    memset(m, 0, 16 * sizeof(float));
    m[0]  = 2.0f / (r - l);
    m[5]  = 2.0f / (t - b);
    m[10] = 1.0f;
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[15] = 1.0f;
}

// ---- DebugDraw ----

void DebugDraw::Create()
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER,   kVertSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragSrc);
    m_prog = glCreateProgram();
    glAttachShader(m_prog, vs);
    glAttachShader(m_prog, fs);
    glLinkProgram(m_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    m_projLoc = glGetUniformLocation(m_prog, "uProj");

    auto makeBuffers = [](GLuint& vao, GLuint& vbo, size_t bytes) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    };
    makeBuffers(m_lineVAO, m_lineVBO, sizeof(m_lineVerts));
    makeBuffers(m_triVAO,  m_triVBO,  sizeof(m_triVerts));
}

void DebugDraw::Destroy()
{
    glDeleteProgram(m_prog);
    glDeleteVertexArrays(1, &m_lineVAO); glDeleteBuffers(1, &m_lineVBO);
    glDeleteVertexArrays(1, &m_triVAO);  glDeleteBuffers(1, &m_triVBO);
    m_prog = 0;
}

void DebugDraw::SetCamera(const Camera2D& cam)
{
    cam.BuildProjectionMatrix(m_proj);
}

// ---- private helpers ----

void DebugDraw::PushLine(b2Vec2 a, b2Vec2 b, b2Color c)
{
    if (m_lineCount + 2 > kMaxVerts) FlushLines();
    m_lineVerts[m_lineCount++] = {a.x, a.y, c.r, c.g, c.b, c.a};
    m_lineVerts[m_lineCount++] = {b.x, b.y, c.r, c.g, c.b, c.a};
}

void DebugDraw::PushTriangle(b2Vec2 a, b2Vec2 b, b2Vec2 c, b2Color col)
{
    if (m_triCount + 3 > kMaxVerts) FlushTriangles();
    m_triVerts[m_triCount++] = {a.x, a.y, col.r, col.g, col.b, col.a};
    m_triVerts[m_triCount++] = {b.x, b.y, col.r, col.g, col.b, col.a};
    m_triVerts[m_triCount++] = {c.x, c.y, col.r, col.g, col.b, col.a};
}

void DebugDraw::FlushLines()
{
    if (m_lineCount == 0) return;
    glUseProgram(m_prog);
    glUniformMatrix4fv(m_projLoc, 1, GL_FALSE, m_proj);
    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_lineCount * (GLsizeiptr)sizeof(Vert), m_lineVerts);
    glDrawArrays(GL_LINES, 0, m_lineCount);
    glBindVertexArray(0);
    m_lineCount = 0;
}

void DebugDraw::FlushTriangles()
{
    if (m_triCount == 0) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_prog);
    glUniformMatrix4fv(m_projLoc, 1, GL_FALSE, m_proj);
    glBindVertexArray(m_triVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_triVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_triCount * (GLsizeiptr)sizeof(Vert), m_triVerts);
    glDrawArrays(GL_TRIANGLES, 0, m_triCount);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    m_triCount = 0;
}

void DebugDraw::Flush()
{
    FlushTriangles();
    FlushLines();
}

// ---- b2Draw interface ----

void DebugDraw::DrawPolygon(const b2Vec2* verts, int32 count, const b2Color& color)
{
    for (int32 i = 0; i < count; ++i)
        PushLine(verts[i], verts[(i + 1) % count], color);
}

void DebugDraw::DrawSolidPolygon(const b2Vec2* verts, int32 count, const b2Color& color)
{
    b2Color fill(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 0.5f);
    for (int32 i = 1; i < count - 1; ++i)
        PushTriangle(verts[0], verts[i], verts[i + 1], fill);
    DrawPolygon(verts, count, color);
}

void DebugDraw::DrawCircle(const b2Vec2& center, float radius, const b2Color& color)
{
    const int   kSegs = 16;
    const float kInc  = 2.0f * b2_pi / float(kSegs);
    for (int i = 0; i < kSegs; ++i)
    {
        float a0 = float(i) * kInc, a1 = float(i + 1) * kInc;
        b2Vec2 p0(center.x + radius * cosf(a0), center.y + radius * sinf(a0));
        b2Vec2 p1(center.x + radius * cosf(a1), center.y + radius * sinf(a1));
        PushLine(p0, p1, color);
    }
}

void DebugDraw::DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color)
{
    const int   kSegs = 16;
    const float kInc  = 2.0f * b2_pi / float(kSegs);
    b2Color fill(color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 0.5f);
    for (int i = 0; i < kSegs; ++i)
    {
        float a0 = float(i) * kInc, a1 = float(i + 1) * kInc;
        b2Vec2 p0(center.x + radius * cosf(a0), center.y + radius * sinf(a0));
        b2Vec2 p1(center.x + radius * cosf(a1), center.y + radius * sinf(a1));
        PushTriangle(center, p0, p1, fill);
        PushLine(p0, p1, color);
    }
    b2Vec2 axisEnd(center.x + radius * axis.x, center.y + radius * axis.y);
    PushLine(center, axisEnd, color);
}

void DebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    PushLine(p1, p2, color);
}

void DebugDraw::DrawTransform(const b2Transform& xf)
{
    const float kLen = 0.4f;
    b2Vec2 p  = xf.p;
    b2Vec2 px(p.x + kLen * xf.q.GetXAxis().x, p.y + kLen * xf.q.GetXAxis().y);
    b2Vec2 py(p.x + kLen * xf.q.GetYAxis().x, p.y + kLen * xf.q.GetYAxis().y);
    PushLine(p, px, b2Color(1.0f, 0.0f, 0.0f));
    PushLine(p, py, b2Color(0.0f, 1.0f, 0.0f));
}

void DebugDraw::DrawPoint(const b2Vec2& p, float size, const b2Color& color)
{
    float hs = size * 0.5f;
    b2Vec2 tl(p.x - hs, p.y + hs), tr(p.x + hs, p.y + hs);
    b2Vec2 bl(p.x - hs, p.y - hs), br(p.x + hs, p.y - hs);
    PushLine(tl, tr, color); PushLine(tr, br, color);
    PushLine(br, bl, color); PushLine(bl, tl, color);
}
