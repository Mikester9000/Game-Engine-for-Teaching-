/**
 * @file CinematicEditorPanel.hpp
 * @brief Cinematic timeline authoring panel (M22).
 */

#pragma once

#include <string>
#include <vector>

class CinematicEditorPanel
{
public:
    void Render();

private:
    struct Keyframe
    {
        float time = 0.0f;
        float position[3] = {0.0f, 0.0f, 0.0f};
        float lookAt[3] = {0.0f, 0.0f, 1.0f};
        float fov = 60.0f;
    };

    struct Shot
    {
        std::string label = "New Shot";
        float duration = 2.0f;
        std::vector<Keyframe> keyframes;
    };

    void RenderTimelineStrip();
    void RenderShotInspector();
    void RenderPreview();
    void EnsureValidSelection();
    void SeedDefaultIfEmpty();
    Keyframe EvaluatePreview(float t) const;

    std::vector<Shot> m_shots;
    int m_selectedShot = -1;
    int m_selectedKeyframe = -1;
    float m_previewTime = 0.0f;
    bool m_playPreview = false;
};
