#include "sb7.h"
#include "box2d/box2d.h"
#include "debug_draw.h"
#include "common/common.h"

// ---- Demo1: Tumbler ----
// 회전하는 상자 안에 작은 박스를 800개까지 쏟아 넣는 시뮬레이션.
// R 키로 리셋.

class TumblerApp : public sb7::application
{
    static constexpr int kMaxCount = 800;

    b2World          m_world{b2Vec2(0.0f, -10.0f)};
    DebugDraw        m_draw;
    Camera2D         m_cam;
    b2RevoluteJoint* m_joint  = nullptr;
    int32            m_count  = 0;

    void BuildScene()
    {
        // 기존 모든 body 제거
        for (b2Body* b = m_world.GetBodyList(); b; )
        {
            b2Body* next = b->GetNext();
            m_world.DestroyBody(b);
            b = next;
        }
        m_joint = nullptr;
        m_count = 0;

        // ground (정적, 회전축 앵커용)
        b2Body* ground = nullptr;
        {
            b2BodyDef bd;
            ground = m_world.CreateBody(&bd);
        }

        // 회전하는 컨테이너
        {
            b2BodyDef bd;
            bd.type       = b2_dynamicBody;
            bd.allowSleep = false;
            bd.position.Set(0.0f, 10.0f);
            b2Body* body = m_world.CreateBody(&bd);

            b2PolygonShape shape;
            shape.SetAsBox(0.5f, 10.0f, b2Vec2( 10.0f, 0.0f), 0.0f);
            body->CreateFixture(&shape, 5.0f);
            shape.SetAsBox(0.5f, 10.0f, b2Vec2(-10.0f, 0.0f), 0.0f);
            body->CreateFixture(&shape, 5.0f);
            shape.SetAsBox(10.0f, 0.5f, b2Vec2(0.0f,  10.0f), 0.0f);
            body->CreateFixture(&shape, 5.0f);
            shape.SetAsBox(10.0f, 0.5f, b2Vec2(0.0f, -10.0f), 0.0f);
            body->CreateFixture(&shape, 5.0f);

            b2RevoluteJointDef jd;
            jd.bodyA          = ground;
            jd.bodyB          = body;
            jd.localAnchorA.Set(0.0f, 10.0f);
            jd.localAnchorB.Set(0.0f, 0.0f);
            jd.referenceAngle = 0.0f;
            jd.motorSpeed     = 0.05f * b2_pi;
            jd.maxMotorTorque = 1e8f;
            jd.enableMotor    = true;
            m_joint = (b2RevoluteJoint*)m_world.CreateJoint(&jd);
        }
    }

public:
    void init() override
    {
        strcpy(info.title, "Box2D Demo1 - Tumbler  |  R: reset");
        info.windowWidth  = 1280;
        info.windowHeight = 720;
        info.majorVersion = 4;
        info.minorVersion = 1;
    }

    void startup() override
    {
        m_cam.center = {0.0f, 10.0f};
        m_cam.zoom   = 0.5f;
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

        // 매 프레임 박스 하나씩 추가
        if (m_count < kMaxCount)
        {
            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position.Set(0.0f, 10.0f);
            b2Body* body = m_world.CreateBody(&bd);
            b2PolygonShape shape;
            shape.SetAsBox(0.125f, 0.125f);
            body->CreateFixture(&shape, 1.0f);
            ++m_count;
        }

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

DECLARE_MAIN(TumblerApp)
