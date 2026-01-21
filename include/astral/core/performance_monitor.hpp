#pragma once
#include <vector>
#include <deque>

namespace astral {

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor() = default;

    void update(float deltaTime);
    void renderUI();

    float getAverageFPS() const { return m_avgFPS; }
    float getFrameTime() const { return m_lastFrameTime; }

private:
    float m_lastFrameTime = 0.0f;
    float m_avgFPS = 0.0f;
    float m_minFPS = 0.0f;
    float m_maxFPS = 0.0f;
    float m_1PercentLowFPS = 0.0f;

    // Settings
    int m_maxHistorySize = 1000;

    std::deque<float> m_frameTimes; // Milliseconds
};

} // namespace astral
