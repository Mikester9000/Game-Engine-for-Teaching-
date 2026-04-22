/**
 * @file CinematicEditorPanel.cpp
 * @brief Cinematic timeline authoring panel implementation (M22).
 */

#include "CinematicEditorPanel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

void CinematicEditorPanel::Render()
{
    // TEACHING NOTE — Dedicated cinematic authoring panel
    // Timeline data is edited in a separate panel so level/entity editing stays
    // uncluttered. This mirrors mainstream toolchains where cut-scene tools are
    // split from scene hierarchy and inspector panels.
    SeedDefaultIfEmpty();
    EnsureValidSelection();

    if (!ImGui::Begin("Cinematic Editor", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    RenderTimelineStrip();
    ImGui::Separator();
    RenderShotInspector();
    ImGui::Separator();
    RenderPreview();

    ImGui::End();
}

void CinematicEditorPanel::RenderTimelineStrip()
{
    if (ImGui::Button("+ Add Shot"))
    {
        Shot shot;
        shot.label = "Shot_" + std::to_string(static_cast<int>(m_shots.size() + 1));
        shot.duration = 2.0f;
        shot.keyframes.push_back(Keyframe{});
        Keyframe end = shot.keyframes.back();
        end.time = shot.duration;
        end.position[2] = -2.0f;
        shot.keyframes.push_back(end);
        m_shots.push_back(shot);
        m_selectedShot = static_cast<int>(m_shots.size()) - 1;
        m_selectedKeyframe = 0;
        m_previewTime = 0.0f;
    }

    ImGui::SameLine();
    if (m_selectedShot < 0)
        ImGui::BeginDisabled();
    if (ImGui::Button("- Remove Shot") && m_selectedShot >= 0)
    {
        m_shots.erase(m_shots.begin() + m_selectedShot);
        EnsureValidSelection();
    }
    if (m_selectedShot < 0)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (m_selectedShot <= 0)
        ImGui::BeginDisabled();
    if (ImGui::Button("Move Up") && m_selectedShot > 0)
    {
        std::swap(m_shots[m_selectedShot], m_shots[m_selectedShot - 1]);
        --m_selectedShot;
    }
    if (m_selectedShot <= 0)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (m_selectedShot < 0 || m_selectedShot >= static_cast<int>(m_shots.size()) - 1)
        ImGui::BeginDisabled();
    if (ImGui::Button("Move Down") && m_selectedShot >= 0 &&
        m_selectedShot < static_cast<int>(m_shots.size()) - 1)
    {
        std::swap(m_shots[m_selectedShot], m_shots[m_selectedShot + 1]);
        ++m_selectedShot;
    }
    if (m_selectedShot < 0 || m_selectedShot >= static_cast<int>(m_shots.size()) - 1)
        ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextUnformatted("Timeline");
    ImGui::BeginChild("##cinematic_timeline", ImVec2(0, 110), ImGuiChildFlags_Borders);
    for (int i = 0; i < static_cast<int>(m_shots.size()); ++i)
    {
        Shot& shot = m_shots[static_cast<size_t>(i)];
        ImGui::PushID(i);
        const bool selected = (i == m_selectedShot);

        char buttonLabel[256];
        std::snprintf(
            buttonLabel,
            sizeof(buttonLabel),
            "[%02d] %s  (%.2fs)",
            i,
            shot.label.c_str(),
            shot.duration
        );

        if (ImGui::Selectable(buttonLabel, selected))
        {
            m_selectedShot = i;
            m_selectedKeyframe = 0;
            m_previewTime = 0.0f;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void CinematicEditorPanel::RenderShotInspector()
{
    if (m_selectedShot < 0 || m_selectedShot >= static_cast<int>(m_shots.size()))
    {
        ImGui::TextDisabled("Select a shot from the timeline.");
        return;
    }

    Shot& shot = m_shots[static_cast<size_t>(m_selectedShot)];

    char labelBuf[128];
    std::strncpy(labelBuf, shot.label.c_str(), sizeof(labelBuf) - 1);
    labelBuf[sizeof(labelBuf) - 1] = '\0';
    ImGui::TextUnformatted("Shot Label");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##shot_label", labelBuf, sizeof(labelBuf)))
        shot.label = labelBuf;

    ImGui::TextUnformatted("Duration (seconds)");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat("##shot_duration", &shot.duration, 0.01f, 0.1f, 120.0f))
    {
        for (Keyframe& key : shot.keyframes)
            key.time = std::min(key.time, shot.duration);
        std::sort(
            shot.keyframes.begin(),
            shot.keyframes.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; }
        );
        m_previewTime = std::min(m_previewTime, shot.duration);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Keyframes");
    if (ImGui::SmallButton("+ Add Keyframe"))
    {
        Keyframe key;
        key.time = shot.duration;
        if (!shot.keyframes.empty())
            key = shot.keyframes.back();
        key.time = shot.duration;
        shot.keyframes.push_back(key);
        std::sort(
            shot.keyframes.begin(),
            shot.keyframes.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; }
        );
        m_selectedKeyframe = static_cast<int>(shot.keyframes.size()) - 1;
    }

    ImGui::SameLine();
    if (m_selectedKeyframe < 0 || m_selectedKeyframe >= static_cast<int>(shot.keyframes.size()))
        ImGui::BeginDisabled();
    if (ImGui::SmallButton("- Remove Keyframe") &&
        m_selectedKeyframe >= 0 &&
        m_selectedKeyframe < static_cast<int>(shot.keyframes.size()))
    {
        shot.keyframes.erase(shot.keyframes.begin() + m_selectedKeyframe);
        EnsureValidSelection();
    }
    if (m_selectedKeyframe < 0 || m_selectedKeyframe >= static_cast<int>(shot.keyframes.size()))
        ImGui::EndDisabled();

    ImGui::BeginChild("##keyframe_list", ImVec2(0, 90), ImGuiChildFlags_Borders);
    for (int i = 0; i < static_cast<int>(shot.keyframes.size()); ++i)
    {
        const bool selected = (i == m_selectedKeyframe);
        const Keyframe& key = shot.keyframes[static_cast<size_t>(i)];
        char row[128];
        std::snprintf(row, sizeof(row), "Key %d  t=%.2f", i, key.time);
        if (ImGui::Selectable(row, selected))
            m_selectedKeyframe = i;
    }
    ImGui::EndChild();

    if (m_selectedKeyframe < 0 || m_selectedKeyframe >= static_cast<int>(shot.keyframes.size()))
        return;

    Keyframe& key = shot.keyframes[static_cast<size_t>(m_selectedKeyframe)];
    ImGui::Spacing();
    ImGui::TextUnformatted("Selected Keyframe");
    ImGui::DragFloat("Time", &key.time, 0.01f, 0.0f, shot.duration);
    ImGui::DragFloat3("Position", key.position, 0.05f);
    ImGui::DragFloat3("Look At", key.lookAt, 0.05f);
    ImGui::DragFloat("FOV", &key.fov, 0.1f, 1.0f, 170.0f);

    std::sort(
        shot.keyframes.begin(),
        shot.keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; }
    );
}

void CinematicEditorPanel::RenderPreview()
{
    if (m_selectedShot < 0 || m_selectedShot >= static_cast<int>(m_shots.size()))
    {
        ImGui::TextDisabled("Preview unavailable.");
        return;
    }

    Shot& shot = m_shots[static_cast<size_t>(m_selectedShot)];
    if (shot.duration <= 0.0f)
    {
        ImGui::TextDisabled("Shot duration must be > 0.");
        return;
    }

    // TEACHING NOTE — Editor-local preview clock
    // The panel keeps a local preview clock for timeline scrubbing and play/
    // pause. Runtime sequencer integration is separate and can consume cooked
    // files; this editor preview keeps authoring feedback immediate.
    if (ImGui::Button(m_playPreview ? "Pause" : "Play"))
        m_playPreview = !m_playPreview;
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        m_playPreview = false;
        m_previewTime = 0.0f;
    }

    if (m_playPreview)
    {
        m_previewTime += ImGui::GetIO().DeltaTime;
        if (m_previewTime >= shot.duration)
        {
            m_previewTime = shot.duration;
            m_playPreview = false;
        }
    }

    ImGui::SliderFloat("Preview Time", &m_previewTime, 0.0f, shot.duration);
    const Keyframe sample = EvaluatePreview(m_previewTime);
    ImGui::Text("Eval Pos: (%.2f, %.2f, %.2f)",
                sample.position[0], sample.position[1], sample.position[2]);
    ImGui::Text("Eval Look: (%.2f, %.2f, %.2f)",
                sample.lookAt[0], sample.lookAt[1], sample.lookAt[2]);
    ImGui::Text("Eval FOV: %.2f", sample.fov);
    ImGui::TextDisabled("Viewport camera hookup uses runtime CinematicSequencer (deferred wiring).");
}

void CinematicEditorPanel::EnsureValidSelection()
{
    if (m_shots.empty())
    {
        m_selectedShot = -1;
        m_selectedKeyframe = -1;
        m_previewTime = 0.0f;
        return;
    }

    m_selectedShot = std::clamp(m_selectedShot, 0, static_cast<int>(m_shots.size()) - 1);
    Shot& shot = m_shots[static_cast<size_t>(m_selectedShot)];
    if (shot.keyframes.empty())
    {
        m_selectedKeyframe = -1;
    }
    else
    {
        m_selectedKeyframe = std::clamp(m_selectedKeyframe, 0, static_cast<int>(shot.keyframes.size()) - 1);
    }
    m_previewTime = std::max(0.0f, std::min(m_previewTime, shot.duration));
}

void CinematicEditorPanel::SeedDefaultIfEmpty()
{
    if (!m_shots.empty())
        return;

    Shot shot;
    shot.label = "intro_pan";
    shot.duration = 3.0f;
    Keyframe start;
    start.time = 0.0f;
    start.position[0] = 0.0f;
    start.position[1] = 5.0f;
    start.position[2] = -10.0f;
    start.lookAt[0] = 0.0f;
    start.lookAt[1] = 1.5f;
    start.lookAt[2] = 0.0f;
    start.fov = 55.0f;

    Keyframe end = start;
    end.time = 3.0f;
    end.position[0] = 4.0f;
    end.position[1] = 4.0f;
    end.position[2] = -6.0f;
    end.lookAt[0] = 0.0f;
    end.lookAt[1] = 1.2f;
    end.lookAt[2] = 1.0f;
    end.fov = 50.0f;

    shot.keyframes.push_back(start);
    shot.keyframes.push_back(end);
    m_shots.push_back(shot);
    m_selectedShot = 0;
    m_selectedKeyframe = 0;
}

CinematicEditorPanel::Keyframe CinematicEditorPanel::EvaluatePreview(float t) const
{
    const Shot& shot = m_shots[static_cast<size_t>(m_selectedShot)];
    if (shot.keyframes.empty())
        return Keyframe{};
    if (shot.keyframes.size() == 1 || t <= shot.keyframes.front().time)
        return shot.keyframes.front();
    if (t >= shot.keyframes.back().time)
        return shot.keyframes.back();

    for (size_t i = 1; i < shot.keyframes.size(); ++i)
    {
        const Keyframe& a = shot.keyframes[i - 1];
        const Keyframe& b = shot.keyframes[i];
        if (t <= b.time)
        {
            const float alpha = (t - a.time) / std::max(0.0001f, b.time - a.time);
            Keyframe out = a;
            for (int axis = 0; axis < 3; ++axis)
            {
                out.position[axis] = a.position[axis] + (b.position[axis] - a.position[axis]) * alpha;
                out.lookAt[axis] = a.lookAt[axis] + (b.lookAt[axis] - a.lookAt[axis]) * alpha;
            }
            out.fov = a.fov + (b.fov - a.fov) * alpha;
            out.time = t;
            return out;
        }
    }

    return shot.keyframes.back();
}
