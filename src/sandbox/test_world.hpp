#pragma once

#include "engine/ecs/ECS.hpp"
#include "engine/core/Logger.hpp"

namespace engine {
namespace sandbox {

class TestWorld
{
public:
    // TEACHING NOTE — Stable integration surface
    // Keep this class API intentionally small and stable because `main.cpp`
    // drives TestWorld in both headless CI and interactive sandbox mode.
    TestWorld();
    ~TestWorld() = default;

    TestWorld(const TestWorld&) = delete;
    TestWorld& operator=(const TestWorld&) = delete;

    bool Init();
    void Update(float dt);
    void GetClearColour(float& r, float& g, float& b) const;
    void PrintStatus() const;

    bool IsDemoComplete() const { return m_demoComplete; }
    bool AllSystemsOk() const { return m_allSystemsOk; }

private:
    void UpdateDemoState(float dt);

    World m_world;
    EntityID m_player = NULL_ENTITY;

    int m_frame = 0;
    float m_time = 0.0f;

    float m_clearR = 0.10f;
    float m_clearG = 0.12f;
    float m_clearB = 0.18f;

    bool m_demoComplete = false;
    bool m_allSystemsOk = false;

    static constexpr int DEMO_FRAMES = 600;
    static constexpr int STATUS_PRINT_INTERVAL = 120;
};

} // namespace sandbox
} // namespace engine
