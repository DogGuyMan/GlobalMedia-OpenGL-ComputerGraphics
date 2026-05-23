#include "sb7.h"
#include "box2d/box2d.h"
#include "debug_draw.h"

// ---- Demo2: Car ----
// 휠 조인트(스프링 서스펜션)를 가진 자동차가 울퉁불퉁한 지형을 달린다.
// A: 전진 / D: 후진 / S: 정지 / R: 리셋

class CarApp : public sb7::application
{
    b2World      m_world{b2Vec2(0.0f, -10.0f)};
    DebugDraw    m_draw;
    Camera2D     m_cam;

    b2Body*      m_car    = nullptr;
    b2Body*      m_wheel1 = nullptr;
    b2Body*      m_wheel2 = nullptr;
    b2WheelJoint* m_spring1 = nullptr;
    b2WheelJoint* m_spring2 = nullptr;
    float        m_speed  = 50.0f;

    void BuildScene()
    {
        for (b2Body* b = m_world.GetBodyList(); b; )
        {
            b2Body* next = b->GetNext();
            m_world.DestroyBody(b);
            b = next;
        }
        m_car = m_wheel1 = m_wheel2 = nullptr;
        m_spring1 = m_spring2 = nullptr;

        b2Body* ground = nullptr;
        {
            b2BodyDef bd;
            ground = m_world.CreateBody(&bd);

            b2EdgeShape shape;
            b2FixtureDef fd;
            fd.shape   = &shape;
            fd.density  = 0.0f;
            fd.friction = 0.6f;

            shape.SetTwoSided(b2Vec2(-20.0f, 0.0f), b2Vec2(20.0f, 0.0f));
            ground->CreateFixture(&fd);

            float hs[10] = {0.25f, 1.0f, 4.0f, 0.0f, 0.0f, -1.0f, -2.0f, -2.0f, -1.25f, 0.0f};
            float x = 20.0f, y1 = 0.0f, dx = 5.0f;
            for (int i = 0; i < 10; ++i)
            {
                float y2 = hs[i];
                shape.SetTwoSided(b2Vec2(x, y1), b2Vec2(x + dx, y2));
                ground->CreateFixture(&fd);
                y1 = y2; x += dx;
            }
            for (int i = 0; i < 10; ++i)
            {
                float y2 = hs[i];
                shape.SetTwoSided(b2Vec2(x, y1), b2Vec2(x + dx, y2));
                ground->CreateFixture(&fd);
                y1 = y2; x += dx;
            }
            shape.SetTwoSided(b2Vec2(x, 0.0f), b2Vec2(x + 40.0f, 0.0f));
            ground->CreateFixture(&fd);
            x += 80.0f;
            shape.SetTwoSided(b2Vec2(x, 0.0f), b2Vec2(x + 40.0f, 0.0f));
            ground->CreateFixture(&fd);
            x += 40.0f;
            shape.SetTwoSided(b2Vec2(x, 0.0f), b2Vec2(x + 10.0f, 5.0f));
            ground->CreateFixture(&fd);
            x += 20.0f;
            shape.SetTwoSided(b2Vec2(x, 0.0f), b2Vec2(x + 40.0f, 0.0f));
            ground->CreateFixture(&fd);
            x += 40.0f;
            shape.SetTwoSided(b2Vec2(x, 0.0f), b2Vec2(x, 20.0f));
            ground->CreateFixture(&fd);
        }

        // 자동차 섀시
        {
            b2PolygonShape chassis;
            b2Vec2 verts[6];
            verts[0].Set(-1.5f, -0.5f); verts[1].Set(1.5f, -0.5f);
            verts[2].Set(1.5f,  0.0f);  verts[3].Set(0.0f,  0.9f);
            verts[4].Set(-1.15f, 0.9f); verts[5].Set(-1.5f, 0.2f);
            chassis.Set(verts, 6);

            b2CircleShape circle;
            circle.m_radius = 0.4f;

            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position.Set(0.0f, 1.0f);
            m_car = m_world.CreateBody(&bd);
            m_car->CreateFixture(&chassis, 1.0f);

            b2FixtureDef fd;
            fd.shape   = &circle;
            fd.density  = 1.0f;
            fd.friction = 0.9f;

            bd.position.Set(-1.0f, 0.35f);
            m_wheel1 = m_world.CreateBody(&bd);
            m_wheel1->CreateFixture(&fd);

            bd.position.Set(1.0f, 0.4f);
            m_wheel2 = m_world.CreateBody(&bd);
            m_wheel2->CreateFixture(&fd);

            b2WheelJointDef jd;
            b2Vec2 axis(0.0f, 1.0f);

            float mass1      = m_wheel1->GetMass();
            float mass2      = m_wheel2->GetMass();
            float hertz      = 4.0f;
            float dampRatio  = 0.7f;
            float omega      = 2.0f * b2_pi * hertz;

            jd.Initialize(m_car, m_wheel1, m_wheel1->GetPosition(), axis);
            jd.motorSpeed        = 0.0f;
            jd.maxMotorTorque    = 20.0f;
            jd.enableMotor       = true;
            jd.stiffness         = mass1 * omega * omega;
            jd.damping           = 2.0f * mass1 * dampRatio * omega;
            jd.lowerTranslation  = -0.25f;
            jd.upperTranslation  = 0.25f;
            jd.enableLimit       = true;
            m_spring1 = (b2WheelJoint*)m_world.CreateJoint(&jd);

            jd.Initialize(m_car, m_wheel2, m_wheel2->GetPosition(), axis);
            jd.motorSpeed        = 0.0f;
            jd.maxMotorTorque    = 10.0f;
            jd.enableMotor       = false;
            jd.stiffness         = mass2 * omega * omega;
            jd.damping           = 2.0f * mass2 * dampRatio * omega;
            jd.lowerTranslation  = -0.25f;
            jd.upperTranslation  = 0.25f;
            jd.enableLimit       = true;
            m_spring2 = (b2WheelJoint*)m_world.CreateJoint(&jd);
        }
    }

public:
    void init() override
    {
        strcpy(info.title, "Box2D Demo2 - Car  |  A:forward  D:reverse  S:stop  R:reset");
        info.windowWidth  = 1280;
        info.windowHeight = 720;
        info.majorVersion = 4;
        info.minorVersion = 1;
    }

    void startup() override
    {
        m_cam.center = {0.0f, 3.0f};
        m_cam.zoom   = 0.25f;
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

        m_world.Step(1.0f / 60.0f, 8, 3);

        // 카메라가 자동차를 수평으로 추적
        if (m_car)
            m_cam.center.x = m_car->GetPosition().x;

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
        // GLFW_PRESS=1, GLFW_REPEAT=2
        if (action != 1 && action != 2) return;

        if (!m_spring1) return;

        switch (key)
        {
        case 65:  // GLFW_KEY_A
            m_spring1->SetMotorSpeed(m_speed);
            break;
        case 83:  // GLFW_KEY_S
            m_spring1->SetMotorSpeed(0.0f);
            break;
        case 68:  // GLFW_KEY_D
            m_spring1->SetMotorSpeed(-m_speed);
            break;
        case 82:  // GLFW_KEY_R
            BuildScene();
            break;
        default:
            break;
        }
    }
};

DECLARE_MAIN(CarApp)
