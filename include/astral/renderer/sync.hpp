#pragma once

#include "astral/core/context.hpp"
#include <vector>
#include <vulkan/vulkan.h>

namespace astral {

class FrameSync {
public:
  FrameSync(Context *context, uint32_t maxFramesInFlight);
  ~FrameSync();

  void waitForFrame(uint32_t frameIndex);
  void resetFence(uint32_t frameIndex);

  const VkSemaphore &getImageAvailableSemaphore(uint32_t frameIndex) const {
    return m_imageAvailableSemaphores[frameIndex];
  }
  const VkSemaphore &getRenderFinishedSemaphore(uint32_t frameIndex) const {
    return m_renderFinishedSemaphores[frameIndex];
  }
  const VkFence &getInFlightFence(uint32_t frameIndex) const {
    return m_inFlightFences[frameIndex];
  }

private:
  Context *m_context;
  uint32_t m_maxFramesInFlight;

  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;
};

} // namespace astral
