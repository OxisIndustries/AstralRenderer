#include "astral/core/performance_monitor.hpp"
#include <imgui.h>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <vector>

namespace astral {

PerformanceMonitor::PerformanceMonitor() {
}

void PerformanceMonitor::update(float deltaTime) {
    // deltaTime is in seconds
    float frameTimeMs = deltaTime * 1000.0f;
    m_lastFrameTime = frameTimeMs;

    m_frameTimes.push_back(frameTimeMs);
    if (m_frameTimes.size() > (size_t)m_maxHistorySize) {
        m_frameTimes.pop_front();
    }

    // Calculate Stats over the history buffer
    if (!m_frameTimes.empty()) {
        float sum = 0.0f;
        float minTime = 100000.0f;
        float maxTime = 0.0f;
        std::vector<float> sortedTimes;
        sortedTimes.reserve(m_frameTimes.size());

        for (float time : m_frameTimes) {
            sum += time;
            if (time < minTime) minTime = time;
            if (time > maxTime) maxTime = time;
            sortedTimes.push_back(time);
        }

        m_avgFPS = 1000.0f / (sum / m_frameTimes.size());
        // FPS is inverse of time. High Frame Time = Low FPS.
        // Min FPS = 1000 / Max Time
        // Max FPS = 1000 / Min Time
        m_minFPS = 1000.0f / maxTime;
        m_maxFPS = 1000.0f / minTime;

        // 1% Low FPS: The FPS value that 99% of frames are faster than.
        // This corresponds to the 99th percentile frame time (slow frames).
        std::sort(sortedTimes.begin(), sortedTimes.end());
        size_t index1Percent = (size_t)(sortedTimes.size() * 0.99f);
        if (index1Percent >= sortedTimes.size()) index1Percent = sortedTimes.size() - 1;
        float time1Percent = sortedTimes[index1Percent];
        m_1PercentLowFPS = 1000.0f / time1Percent;
    }
}

void PerformanceMonitor::renderUI() {
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver); // Default size
    if (ImGui::Begin("Performance Statistics", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("FPS: %.1f", m_avgFPS);
        ImGui::SameLine();
        ImGui::TextDisabled("  (%.3f ms)", m_lastFrameTime);

        ImGui::Separator();
        ImGui::Columns(4, "PerfMetrics", false);
        ImGui::Text("Avg"); ImGui::NextColumn();
        ImGui::Text("Min"); ImGui::NextColumn();
        ImGui::Text("Max"); ImGui::NextColumn();
        ImGui::Text("1%% Low"); ImGui::NextColumn();
        
        ImGui::Text("%.1f", m_avgFPS); ImGui::NextColumn();
        ImGui::Text("%.1f", m_minFPS); ImGui::NextColumn();
        ImGui::Text("%.1f", m_maxFPS); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%.1f", m_1PercentLowFPS); ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::Separator();

        // Plot
        if (!m_frameTimes.empty()) {
            // Plot needs a contiguous array
            std::vector<float> linearArray(m_frameTimes.begin(), m_frameTimes.end());
            
            // Auto-scaling graph height for visibility, but keep 0 base
            float maxGraphTime = 33.3f; // Default 30 FPS line
            for(float t : linearArray) if(t > maxGraphTime) maxGraphTime = t;
            
            ImGui::PlotLines("##FrameTimes", linearArray.data(), (int)linearArray.size(), 0, "Frame Time (ms)", 0.0f, maxGraphTime * 1.1f, ImVec2(0, 80));
        }
        
        ImGui::TextDisabled("History: %d frames", (int)m_frameTimes.size());
    }
    ImGui::End();
}

} // namespace astral
