#include "sandbox/test_world.hpp"

#include <cmath>
#include <iostream>

namespace engine {
namespace sandbox {

namespace {
constexpr float TESTWORLD_MOVE_SPEED = 1.5f;
constexpr float TESTWORLD_SINE_FREQUENCY = 0.8f;
constexpr float TESTWORLD_SINE_AMPLITUDE = 2.0f;

constexpr float TESTWORLD_PULSE_BASE = 0.5f;
constexpr float TESTWORLD_PULSE_SCALE = 0.5f;
constexpr float TESTWORLD_PULSE_FREQUENCY = 1.2f;

constexpr float TESTWORLD_RED_BASE = 0.06f;
constexpr float TESTWORLD_RED_VARIATION = 0.30f;
constexpr float TESTWORLD_GREEN_BASE = 0.10f;
constexpr float TESTWORLD_GREEN_VARIATION = 0.25f;
constexpr float TESTWORLD_BLUE_BASE = 0.16f;
constexpr float TESTWORLD_BLUE_VARIATION = 0.35f;
} // namespace

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

    // Touch AudioSourceComponent to validate ECS/audio component wiring still
    // compiles and runs after sandbox-side integration changes.
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
    tf.position.x += dt * TESTWORLD_MOVE_SPEED;
    tf.position.y = std::sin(m_time * TESTWORLD_SINE_FREQUENCY) * TESTWORLD_SINE_AMPLITUDE;

    const float pulse = TESTWORLD_PULSE_BASE
                      + TESTWORLD_PULSE_SCALE * std::sin(m_time * TESTWORLD_PULSE_FREQUENCY);
    m_clearR = TESTWORLD_RED_BASE + pulse * TESTWORLD_RED_VARIATION;
    m_clearG = TESTWORLD_GREEN_BASE + pulse * TESTWORLD_GREEN_VARIATION;
    m_clearB = TESTWORLD_BLUE_BASE + pulse * TESTWORLD_BLUE_VARIATION;
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
