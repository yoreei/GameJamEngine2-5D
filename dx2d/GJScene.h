#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <bitset>
#include <random>
#include <fstream>
#include <cmath>
#include <numbers>

#include <DirectXMath.h>

#include "Animation.h"

using namespace DirectX;

BOOST_DEFINE_ENUM_CLASS(State, MAINMENU, PREGAME, INGAME, PAUSED, LOSS, WIN, size);

/* Simulation writes to GameplayState, Renderer displays only */
struct GameplayState {
    std::string map         = "no_map";
    uint64_t    width       = 0;
    uint64_t    height      = 0;
    std::string fileName    = "no_mapFile";
    State       state       = State::MAINMENU;
    bool        explodeCd   = false;
    bool        qLeapCd     = false;
    bool        qLeapActive = false;
    uint64_t    hiScore     = 0;
    uint64_t    points      = 100;

    const char& getTile(size_t x, size_t y) const { return map[y * width + x]; }
};

enum class EntityType {
    PlayerEntity1 = 0,
    PlayerEntity2,
    PlayerEntity3,
    PlayerEntity4,
    EnemyEntity1,
    EnemyEntity2,
    EnemyEntity3,
    EnemyEntity4
};

struct Entity {
    void moveBy(const XMVECTOR& by) { position += by; }
    void setX(float x) { XMVectorSetX(position, x); }
    void setY(float y) { XMVectorSetY(position, y); }
    void interpolate(const Entity& e1, const Entity& e2, float alpha) {
        size     = e1.size * (1.f - alpha) + e2.size * alpha;
        momentum = XMVector4Normalize(XMVectorLerp(e1.momentum, e2.momentum, alpha));
        position = XMVector4Normalize(XMVectorLerp(e1.position, e2.position, alpha));
    }

    bool collides(const Entity& other, float cheatParam = 0.f) {
        XMVECTOR delta         = XMVectorAbs(position - other.position);
        float    lim           = size + other.size - cheatParam;
        uint32_t compareResult = 0;
        XMVectorGreaterR(&compareResult, delta, XMVECTOR{ lim, lim, lim, lim });
        if (XMComparisonAllFalse(compareResult)) {
            return true;
        } else {
            return false;
        }
    }

public:
    XMVECTOR momentum{};
    XMVECTOR position{};
    // v * Interpolatable:
    float size = 2.f;
    // v * Non-Interpolatable:
    uint16_t health = 1;
};

struct GJScene {
    GJScene() { }

    void resetEntities() {
        entities.fill(Entity{});
        for (int i = 0; i < entities.size(); ++i) {
            entities[i].health   = 1;
            entities[i].size     = 2;
            entities[i].position = { 180.f, 240.f, 0.f, 0.f };
        }
    }

    void resetObstacles() { obstacles.fill(Entity{}); }


    // int ScrH() const {
    //	return scr_height;
    // }
    // int ScrW() const {
    //	return scr_width;
    // }
    // int HscrH() const {
    //	return hscr_height;
    // }
    // int HscrW() const {
    //	return hscr_width;
    // }
    // float ScrHf() const {
    //	return scr_height;
    // }
    // float ScrWf() const {
    //	return scr_width;
    // }
    // float HscrHf() const {
    //	return hscr_height;
    // }
    // float HscrWf() const {
    //	return hscr_width;
    // }
    ////v pixels, aspect ratio
    // void setResolution(int height, float ar) {
    //	scr_height = height;
    //	scr_width = int(std::round(height * ar));
    //	hscr_height = scr_height / 2;
    //	hscr_width = scr_width / 2;
    // }


    void interpolate(const GJScene& other, float alpha) {
        for (int i = 0; i < entities.size(); ++i) {
            entities[i].interpolate(entities[i], other.entities[i], alpha);
        }
        for (int i = 0; i < obstacles.size(); ++i) {
            obstacles[i].interpolate(obstacles[i], other.obstacles[i], alpha);
        }
        camera.interpolate(camera, other.camera, alpha);
    }

public:
    // v * Non-interpolatable:

    // v * Interpolatable (4):
    std::array<Entity, 4>  entities{}; //< controlable
    std::array<Entity, 25> obstacles{};

    struct Camera {
        Camera() {
            setFov(1.f); // 70deg
            setDirectionAngle(1);
            setPitch(0);
        }
        XMVECTOR position{ 0, 0, 0, 0 }; //< 2D position. Only x and y are used. See `camHeight`

        // v we keep camHeight out of `position` to not interfere with raycasting intersections (position should stay 2D)
        float camHeight = 0.5f;
        float pitch     = 0; //< radians. We keep pitch out of `directionVector` to not interfere with raycasting in 2D
        // v radians
        void setDirectionAngle(float _angle) {
            directionAngle  = std::fmodf(_angle, 2 * fPi);
            directionVector = XMVECTOR{ std::cosf(directionAngle), std::sinf(directionAngle), 0.f, 0.f };
            spdlog::info("angle: {}, dirx: {}, diry: {}\n",
                         directionAngle,
                         XMVectorGetX(directionVector),
                         XMVectorGetY(directionVector));
        }

        // v radians
        float getDirectionAngle() const { return directionAngle; }

        void setDirectionVector(const XMVECTOR& UNUSED(newDir)) { ASSERT_EXPR(false, "not implemented yet"); }

        XMVECTOR getDirectionVector() const { return directionVector; }


        void setFov(float newFov) {
            fov  = newFov;
            vfov = fov; // todo is this correct for top/bottom letterboxing?

            // half of image plane width if imagePlaneDistance were 1. a is side opposite fov/2. imagePlaneDistance is hypotenuse
            float a = std::tan(fov / 2.f);

            // extend imagePlaneDistance until a = 0.5. Note: image plane width is 1
            imagePlaneDistance = 0.5f / a;
        }

        // v radians
        const float getFov() const { return fov; }
        // v radians
        const float getVfov() const { return vfov; }

        // v image plane width is 1
        float getImagePlaneDistance() const { return imagePlaneDistance; }

        void  setPitch(float _pitch) { pitch = _pitch; }
        float getPitch() const { return pitch; }

        void interpolate(const Camera& c1, const Camera& c2, float alpha) {
            position  = XMVectorLerp(c1.position, c2.position, alpha);
            camHeight = c1.camHeight * (1.f - alpha) + c2.camHeight * alpha;

            setDirectionAngle(c1.directionAngle * (1.f - alpha) + c2.directionAngle * alpha);
            pitch = c1.pitch * (1.f - alpha) + c2.pitch * alpha;
        }

    private:
        // directionVector and directionAngle represent the same thing, we keep both for caching
        XMVECTOR directionVector{ 0, -1, 0, 0 }; //< 2D normalized vector, representing angle
        float    fov;                            //< radians
        float    vfov;                           //< radians

        // image plane is 1 unit wide and distance to camera is controlled via setFov()
        float imagePlaneDistance;

        float directionAngle; //< radians

    } camera;
};
