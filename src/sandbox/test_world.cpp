#include "sandbox/test_world.hpp"

#include <cmath>
#include <iostream>

namespace engine {
namespace sandbox {

TestWorld::TestWorld() = default;

bool TestWorld::Init()
{
    // TEACHING NOTE — Keep TestWorld as a low-risk integration smoke test.
    // It should depend only on stable APIs so CI can quickly verify that
    // engine_sandbox starts, updates ECS state, and renders a deterministic frame.
    RegisterAllComponents(m_world);

    m_player = m_world.CreateEntity();

    auto& tf = m_world.AddComponent<TransformComponent>(m_player);
    tf.position = {0.0f, 0.0f, 0.0f};

    auto& hp = m_world.AddComponent<HealthComponent>(m_player);
    hp.hp = 100;
    hp.maxHp = 100;
    hp.mp = 50;
    hp.maxMp = 50;

    auto& nm = m_world.AddComponent<NameComponent>(m_player);
    nm.name = "Noctis";
    nm.internalID = "testworld_player";

    auto& mv = m_world.AddComponent<MovementComponent>(m_player);
    mv.moveSpeed = 4.0f;

    // Touch M3 ECS additions to validate compile/runtime wiring.
    auto& audio = m_world.AddComponent<AudioSourceComponent>(m_player);
    audio.clipID = "guid-sfx-footstep";
    audio.volume = 0.5f;
    audio.isPlaying = false;

    m_allSystemsOk = true;
    PrintStatus();
    return true;
}

void TestWorld::Update(float dt)
{
    ++m_frame;
    m_time += dt;

    UpdateDemoState(dt);

    if (m_frame % STATUS_PRINT_INTERVAL == 0)
    {
        PrintStatus();
    }

    if (m_frame >= DEMO_FRAMES)
    {
        m_demoComplete = true;
    }
}

void TestWorld::UpdateDemoState(float dt)
{
    if (!m_world.IsAlive(m_player) || !m_world.HasComponent<TransformComponent>(m_player))
    {
        m_allSystemsOk = false;
        return;
    }

    auto& tf = m_world.GetComponent<TransformComponent>(m_player);

    // TEACHING NOTE — Simple deterministic motion is ideal for CI checks.
    // A predictable sine-wave path gives a visible animated signal in windowed
    // runs while remaining fully deterministic in headless mode.
    tf.position.x += dt * 1.5f;
    tf.position.y = std::sin(m_time * 0.8f) * 2.0f;

    const float pulse = 0.5f + 0.5f * std::sin(m_time * 1.2f);
    m_clearR = 0.06f + pulse * 0.30f;
    m_clearG = 0.10f + pulse * 0.25f;
    m_clearB = 0.16f + pulse * 0.35f;
}

void TestWorld::GetClearColour(float& r, float& g, float& b) const
{
    r = m_clearR;
    g = m_clearG;
    b = m_clearB;
}

void TestWorld::PrintStatus() const
{
    if (!m_world.IsAlive(m_player) || !m_world.HasComponent<TransformComponent>(m_player))
    {
        std::cout << "[TestWorld] Invalid player entity state.\n";
        return;
    }

    const auto& tf = m_world.GetComponent<TransformComponent>(m_player);
    std::cout << "[TestWorld] frame=" << m_frame
              << " pos=(" << tf.position.x << ", " << tf.position.y << ")"
              << " clear=(" << m_clearR << ", " << m_clearG << ", " << m_clearB << ")\n";
}

} // namespace sandbox
} // namespace engine
