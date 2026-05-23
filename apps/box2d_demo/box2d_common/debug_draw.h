#ifndef __BOX2D_COMMON_DEBUG_DRAW_H__
#define __BOX2D_COMMON_DEBUG_DRAW_H__

#include "box2d/b2_draw.h"
#include <GL/gl3w.h>

struct Camera2D
{
    b2Vec2 center = {0.0f, 10.0f};
    float  zoom   = 1.0f;
    int    width  = 1280;
    int    height = 720;

    void BuildProjectionMatrix(float* m) const;
};

class DebugDraw : public b2Draw
{
public:
    void Create();
    void Destroy();
    void SetCamera(const Camera2D& cam);
    void Flush();

    void DrawPolygon(const b2Vec2* verts, int32 count, const b2Color& color) override;
    void DrawSolidPolygon(const b2Vec2* verts, int32 count, const b2Color& color) override;
    void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) override;
    void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) override;
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
    void DrawTransform(const b2Transform& xf) override;
    void DrawPoint(const b2Vec2& p, float size, const b2Color& color) override;

private:
    void PushLine(b2Vec2 a, b2Vec2 b, b2Color c);
    void PushTriangle(b2Vec2 a, b2Vec2 b, b2Vec2 c, b2Color col);
    void FlushLines();
    void FlushTriangles();

    struct Vert { float x, y, r, g, b, a; };
    static constexpr int kMaxVerts = 3 * 512;

    Vert   m_lineVerts[kMaxVerts];
    int    m_lineCount = 0;
    GLuint m_lineVAO   = 0, m_lineVBO = 0;

    Vert   m_triVerts[kMaxVerts];
    int    m_triCount = 0;
    GLuint m_triVAO   = 0, m_triVBO = 0;

    GLuint m_prog    = 0;
    GLint  m_projLoc = -1;
    float  m_proj[16] = {};
};

#endif
