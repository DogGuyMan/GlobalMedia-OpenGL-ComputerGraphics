#include "sb7.h"
#include "box2d/box2d.h"
#include "debug_draw.h"
#include "common/common.h"

// ---- Demo3: Bridge ----
// 30개 판자를 revolute joint로 연결한 현수교 + 삼각형/원 낙하 오브젝트.
// R 키로 리셋.

class BridgeApp : public sb7::application
{
    static constexpr int kPlanks = 30;

    b2World   m_world{b2Vec2(0.0f, -10.0f)};
    DebugDraw m_draw;
    Camera2D  m_cam;

    void BuildScene()
    {
        for (b2Body* b = m_world.GetBodyList(); b; )
        {
            b2Body* next = b->GetNext();
            m_world.DestroyBody(b);
            b = next;
        }

        b2Body* ground = nullptr;
        {
            b2BodyDef bd;
            ground = m_world.CreateBody(&bd);

            b2EdgeShape shape;
            shape.SetTwoSided(b2Vec2(-40.0f, 0.0f), b2Vec2(40.0f, 0.0f));
            ground->CreateFixture(&shape, 0.0f);
        }

        // 현수교 판자
        {
            b2PolygonShape shape;
            shape.SetAsBox(0.5f, 0.125f);

            b2FixtureDef fd;
            fd.shape   = &shape;
            fd.density  = 20.0f;
            fd.friction = 0.2f;

            b2RevoluteJointDef jd;
            b2Body* prev = ground;

            for (int i = 0; i < kPlanks; ++i)
            {
                b2BodyDef bd;
                bd.type = b2_dynamicBody;
                bd.position.Set(-14.5f + float(i), 5.0f);
                b2Body* body = m_world.CreateBody(&bd);
                body->CreateFixture(&fd);

                b2Vec2 anchor(-15.0f + float(i), 5.0f);
                jd.Initialize(prev, body, anchor);
                m_world.CreateJoint(&jd);

                prev = body;
            }
            b2Vec2 anchor(-15.0f + float(kPlanks), 5.0f);
            jd.Initialize(prev, ground, anchor);
            m_world.CreateJoint(&jd);
        }

        // 삼각형 2개
        for (int i = 0; i < 2; ++i)
        {
            b2Vec2 verts[3];
            verts[0].Set(-0.5f, 0.0f);
            verts[1].Set( 0.5f, 0.0f);
            verts[2].Set( 0.0f, 1.5f);

            b2PolygonShape shape;
            shape.Set(verts, 3);

            b2FixtureDef fd;
            fd.shape  = &shape;
            fd.density = 1.0f;

            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position.Set(-8.0f + float(i) * 8.0f, 12.0f);
            b2Body* body = m_world.CreateBody(&bd);
            body->CreateFixture(&fd);
        }

        // 원 3개
        for (int i = 0; i < 3; ++i)
        {
            b2CircleShape shape;
            shape.m_radius = 0.5f;

            b2FixtureDef fd;
            fd.shape  = &shape;
            fd.density = 1.0f;

            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position.Set(-6.0f + float(i) * 6.0f, 10.0f);
            b2Body* body = m_world.CreateBody(&bd);
            body->CreateFixture(&fd);
        }
    }

public:
    void init() override
    {
        strcpy(info.title, "Box2D Demo3 - Bridge  |  R: reset");
        info.windowWidth  = 1280;
        info.windowHeight = 720;
        info.majorVersion = 4;
        info.minorVersion = 1;
    }

    void startup() override
    {
        m_cam.center = {0.0f, 5.0f};
        m_cam.zoom   = 0.4f;
        m_cam.width  = info.windowWidth;
        m_cam.height = info.windowHeight;

        m_draw.Create();
        m_draw.SetFlags(b2Draw::e_shapeBit | b2Draw::e_jointBit);
        m_world.SetDebugDraw(&m_draw);

        BuildScene();
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    }

    void render(double /*t*/) override
    {
        glClear(GL_COLOR_BUFFER_BIT);

        m_world.Step(SJH::FixedTime(), 8, 3);

        m_draw.SetCamera(m_cam);
        m_world.DebugDraw();
        m_draw.Flush();
    }

    void shutdown() override
    {
        m_draw.Destroy();
    }

    void onKey(int key, int action) override
    {
        // GLFW_KEY_R = 82, GLFW_PRESS = 1
        if (action == 1 && key == 82)
            BuildScene();
    }
};

DECLARE_MAIN(BridgeApp)
