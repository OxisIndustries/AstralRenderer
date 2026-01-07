#include "astral/renderer/ui_manager.hpp"
#include "astral/platform/window.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace astral {

UIManager::UIManager(Context* context, VkRenderPass renderPass) : m_context(context) {
    // 1: Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(m_context->getDevice(), &pool_info, nullptr, &m_imguiPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create imgui descriptor pool");
    }

    // 2: Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Requires docking branch

    ImGui::StyleColorsDark();

    // 3: Initialize backends
    ImGui_ImplGlfw_InitForVulkan(m_context->getWindow().getNativeWindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_context->getInstance();
    init_info.PhysicalDevice = m_context->getPhysicalDevice();
    init_info.Device = m_context->getDevice();
    init_info.Queue = m_context->getGraphicsQueue();
    init_info.DescriptorPool = m_imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.UseDynamicRendering = true;
    
    // Dynamic rendering requires specifying formats
    VkPipelineRenderingCreateInfo renderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM; // Swapchain format
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &swapchainFormat;
    init_info.PipelineRenderingCreateInfo = renderingCreateInfo;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("failed to initialize imgui vulkan backend");
    }

    // Upload fonts
    // Note: With modern ImGui, this is handled during the first frame or via ImGui_ImplVulkan_CreateFontsTexture
}

UIManager::~UIManager() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_context->getDevice(), m_imguiPool, nullptr);
}

void UIManager::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::endFrame() {
    ImGui::Render();
}

void UIManager::render(VkCommandBuffer cmd) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace astral
