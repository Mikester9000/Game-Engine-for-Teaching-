#include "sandbox/test_world.hpp"

#include <cmath>
#include <iostream>

namespace engine {
namespace sandbox {

namespace {
constexpr float kMoveSpeed = 1.5f;
constexpr float kSineFrequency = 0.8f;
constexpr float kSineAmplitude = 2.0f;

constexpr float kPulseBase = 0.5f;
constexpr float kPulseScale = 0.5f;
constexpr float kPulseFrequency = 1.2f;

constexpr float kRedBase = 0.06f;
constexpr float kRedVariation = 0.30f;
constexpr float kGreenBase = 0.10f;
constexpr float kGreenVariation = 0.25f;
constexpr float kBlueBase = 0.16f;
constexpr float kBlueVariation = 0.35f;
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
    // TEACHING NOTE — Placeholder clip GUID
    // The clip ID here is a non-fatal placeholder for sandbox smoke testing.
    // If the backend cannot resolve it through AssetDB, playback simply no-ops.
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
    tf.position.x += dt * kMoveSpeed;
    tf.position.y = std::sin(m_time * kSineFrequency) * kSineAmplitude;

    // TEACHING NOTE — Oscillation formula for deterministic visual feedback
    // Uses base + amplitude * sin(frequency * t) for a smooth periodic pulse.
    // Choosing a pulse frequency different from movement frequency creates a
    // subtle beat effect, making state motion easier to observe in test runs.
    const float pulse = kPulseBase + kPulseScale * std::sin(m_time * kPulseFrequency);
    m_clearR = kRedBase + pulse * kRedVariation;
    m_clearG = kGreenBase + pulse * kGreenVariation;
    m_clearB = kBlueBase + pulse * kBlueVariation;
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
