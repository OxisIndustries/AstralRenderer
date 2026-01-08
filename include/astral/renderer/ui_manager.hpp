#pragma once

#include "astral/core/context.hpp"
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace astral {

class UIManager {
public:
    UIManager(Context* context, VkFormat swapchainFormat);
    ~UIManager();

    void beginFrame();
    void endFrame();
    void render(VkCommandBuffer cmd);

private:
    Context* m_context;
    VkDescriptorPool m_imguiPool;
    VkFormat m_uiFormat;
};

} // namespace astral
