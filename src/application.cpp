#include "astral/application.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <cstdio>
#include <spdlog/spdlog.h>

namespace astral {

Application::Application() { init(); }

Application::~Application() { cleanup(); }

void Application::init() {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  spdlog::set_level(spdlog::level::debug);
  spdlog::info("Starting Astral Renderer Sandbox (Refactored)...");

  WindowSpecs specs;
  specs.title = "Astral Renderer - glTF PBR Sandbox";
  specs.width = 1600;
  specs.height = 900;

  m_window = std::make_unique<Window>(specs);
  m_context = std::make_unique<Context>(m_window.get());
  m_swapchain = std::make_unique<Swapchain>(m_context.get(), m_window.get());
  m_sync = std::make_unique<FrameSync>(m_context.get(), 2);

  // Command Pool
  m_commandPool = std::make_unique<CommandPool>(
      m_context.get(),
      m_context->getQueueFamilyIndices().graphicsFamily.value());

  for (int i = 0; i < 2; i++) { // Max frames in flight
    m_commandBuffers.push_back(m_commandPool->allocateBuffer());
  }

  // Per-Image Semaphores
  m_imageSemaphores.resize(m_swapchain->getImages().size());
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  for (size_t i = 0; i < m_imageSemaphores.size(); i++) {
    if (vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr,
                          &m_imageSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create image semaphore!");
    }
  }

  m_sceneManager = std::make_unique<SceneManager>(m_context.get());
  m_envManager = std::make_unique<EnvironmentManager>(m_context.get());
  m_uiManager = std::make_unique<UIManager>(m_context.get(), m_swapchain->getImageFormat());
  m_loader = std::make_unique<GltfLoader>(m_context.get());

  // Renderer System Init
  m_renderer = std::make_unique<RendererSystem>(
      m_context.get(), m_swapchain.get(), specs.width, specs.height);

  // Initialize Resources
  // Descriptors are registered inside RendererSystem::initializePipelines or
  // via separate register call Note: In current main.cpp, descriptor layouts
  // were needed for pipeline creation. So RendererSystem likely does this in
  // its ctor or init.

  // In the original code, the Layout was retrieved via
  // context.getDescriptorManager().getLayout()
  VkDescriptorSetLayout setLayouts[] = {
      m_context->getDescriptorManager().getLayout()};
  m_renderer->initializePipelines(setLayouts, 1);

  initScene();

  // Input Callbacks
  static Application* s_app = this;
  s_app = this; // Ensure it's set
  
  // ImGui installs its own callbacks in m_uiManager constructor via ImGui_ImplGlfw_InitForVulkan(..., true).
  // We MUST chain them if we want to provide our own.
  
  static GLFWcursorposfun s_prevCursorPosCallback = nullptr;
  s_prevCursorPosCallback = glfwSetCursorPosCallback(m_window->getNativeWindow(), [](GLFWwindow *window, double xpos, double ypos) {
    if (s_prevCursorPosCallback) s_prevCursorPosCallback(window, xpos, ypos);

    static double lastX = 800.0, lastY = 450.0;
    static bool firstMouse = true;
    if (firstMouse) {
      lastX = xpos;
      lastY = ypos;
      firstMouse = false;
    }
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
      s_app->m_camera.processMouse(static_cast<float>(xoffset),
                                 static_cast<float>(yoffset));
    }
  });

  glfwSetInputMode(m_window->getNativeWindow(), GLFW_CURSOR,
                   GLFW_CURSOR_NORMAL);

  spdlog::info("Application Initialized.");
}

void Application::initScene() {
  // Load Skybox
  std::string hdrPath = "assets/textures/skybox.hdr";
  if (std::filesystem::exists(hdrPath)) {
    m_envManager->loadHDR(hdrPath);
  } else {
    spdlog::warn("Skybox HDR not found at: {}. IBL will be disabled.", hdrPath);
  }

  MaterialMetadata defaultMat;
  defaultMat.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  defaultMat.metallicFactor = 0.5f;
  defaultMat.roughnessFactor = 0.5f;
  defaultMat.baseColorTextureIndex = -1;
  defaultMat.metallicRoughnessTextureIndex = -1;
  defaultMat.normalTextureIndex = -1;
  defaultMat.occlusionTextureIndex = -1;
  defaultMat.emissiveTextureIndex = -1;
  defaultMat.alphaCutoff = 0.5f;
  m_sceneManager->addMaterial(defaultMat);

  // Camera
  m_camera.setPerspective(
      45.0f, (float)m_window->getWidth() / (float)m_window->getHeight(), 0.1f,
      1000.0f);
  m_camera.setPosition(glm::vec3(0.0f, 0.0f, 5.0f));

  // Load Model
  m_model = m_loader->loadFromFile("assets/models/damaged_helmet/scene.gltf",
                                   m_sceneManager.get());
  if (!m_model) {
    spdlog::warn("Model not found, creating fallback (empty)...");
  }

  // Default Lights
  {
    astral::Light sun;
    sun.position = glm::vec4(5.0f, 8.0f, 5.0f, 1.0f); // Point light
    sun.color = glm::vec4(1.0f, 1.0f, 1.0f, 10.0f);
    sun.direction = glm::vec4(0.0f, -1.0f, 0.0f, 20.0f);
    sun.params = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    m_sceneManager->addLight(sun);

    astral::Light blueLight;
    blueLight.position = glm::vec4(-5.0f, 2.0f, -5.0f, 0.0f);
    blueLight.color = glm::vec4(0.2f, 0.4f, 1.0f, 5.0f);
    blueLight.direction = glm::vec4(0.0f, 0.0f, 0.0f, 15.0f);
    m_sceneManager->addLight(blueLight);
  }
}

void Application::run() {
  RenderGraph graph(m_context.get());
  m_lastFrameTime = (float)glfwGetTime();

  spdlog::info("Entering Main Loop...");

  while (!m_window->shouldClose()) {
    m_window->pollEvents();
    graph.clear();

    float currentTime = (float)glfwGetTime();
    float deltaTime = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;

    handleInput(deltaTime);
    updateUI(deltaTime);

    // Update Scene Data
    SceneData sd;
    sd.view = m_camera.getViewMatrix();
    sd.proj = m_camera.getProjectionMatrix();
    sd.viewProj = sd.proj * sd.view;
    sd.invView = glm::inverse(sd.view);
    sd.invProj = glm::inverse(sd.proj);
    sd.cameraPos = glm::vec4(m_camera.getPosition(), 1.0f);

    // TAA Jitter (Simple Halton)
    auto halton = [](int index, int base) {
      float f = 1.0f, r = 0.0f;
      while (index > 0) {
        f = f / base;
        r = r + f * (index % base);
        index = index / base;
      }
      return r;
    };
    glm::vec2 jitter = glm::vec2((halton((m_frameIndex % 16) + 1, 2) - 0.5f) /
                                     (float)m_window->getWidth(),
                                 (halton((m_frameIndex % 16) + 1, 3) - 0.5f) /
                                     (float)m_window->getHeight());
    sd.jitter = jitter;
    sd.proj[2][0] += jitter.x;
    sd.proj[2][1] += jitter.y;
    sd.viewProj = sd.proj * sd.view;

    if (m_firstFrame) {
      sd.prevViewProj = sd.viewProj;
      m_firstFrame = false;
    } else {
      sd.prevViewProj = m_prevSceneData.viewProj;
    }
    m_prevSceneData = sd;
    m_frameIndex++;

    // Frustum Planes logic... (Simplified for now, assume Renderer handles
    // culling or we just copy it)
    // ... Copying logic
    glm::mat4 vp = sd.viewProj;
    for (int i = 0; i < 4; i++)
      sd.frustumPlanes[0][i] = vp[i][3] + vp[i][0];
    for (int i = 0; i < 4; i++)
      sd.frustumPlanes[1][i] = vp[i][3] - vp[i][0];
    for (int i = 0; i < 4; i++)
      sd.frustumPlanes[2][i] = vp[i][3] + vp[i][1];
    for (int i = 0; i < 4; i++)
      sd.frustumPlanes[3][i] = vp[i][3] - vp[i][1];
    for (int i = 0; i < 4; i++)
      sd.frustumPlanes[4][i] = vp[i][3] + vp[i][2];
    for (int i = 0; i < 4; i++)
      sd.frustumPlanes[5][i] = vp[i][3] - vp[i][2];
    for (int i = 0; i < 6; i++) {
      float length = glm::length(glm::vec3(sd.frustumPlanes[i]));
      sd.frustumPlanes[i] /= length;
    }

    // Shadows Logic (Light Space Matrix + CSM)
    // ... (Omitting full CSM math here for brevity, assume we need to implement
    // it to match main.cpp exact logic) Ideally this should be in SceneManager
    // or Renderer. For refactor 1:1, I will put it in
    // RendererSystem::setupRenderGraph or keep it here and pass fully formed
    // SceneData. It modifies SceneData heavily. Let's keep it here for now as
    // part of "Update Logic".

    auto &lights = m_sceneManager->getLights();
    glm::vec3 lightPos = glm::vec3(5.0f, 8.0f, 5.0f);
    glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
    bool isDirectional = true;
    if (!lights.empty()) {
      isDirectional = (lights[0].position.w == 1.0f);
      if (isDirectional) {
        lightDir = glm::normalize(glm::vec3(lights[0].direction));
        lightPos = -lightDir * 10.0f;
      } else {
        lightPos = glm::vec3(lights[0].position);
        lightDir = glm::normalize(glm::vec3(0.0f) - lightPos);
      }
    }
    glm::mat4 lightView =
        glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
    float orthoSize = 10.0f;
    glm::mat4 lightProjection =
        glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 100.0f);
    lightProjection[1][1] *= -1;
    sd.lightSpaceMatrix = lightProjection * lightView;

    // CSM Logic copy
    float nearClip = m_camera.getNear();
    float farClip = m_camera.getFar();
    float cascadeSplits[4];
    float lambda = m_uiParams.csmLambda;
    float ratio = farClip / nearClip;

    for (int i = 0; i < 4; i++) {
      float p = (i + 1) / 4.0f;
      float log = nearClip * std::pow(ratio, p);
      float uniform = nearClip + (farClip - nearClip) * p;
      float d = lambda * (log - uniform) + uniform;
      cascadeSplits[i] = d;
    }
    sd.cascadeSplits = glm::vec4(cascadeSplits[0], cascadeSplits[1],
                                 cascadeSplits[2], cascadeSplits[3]);

    // Full CSM loop would be here... (Simplified: copying logic)
    // ... (Logic to compute sd.cascadeViewProj[i])
    // To ensure compilation, I will implement a simplified version or the full
    // version if needed. Let's implement the loop as it is critical for
    // Shadows.
    float lastSplitDist = nearClip;
    for (int i = 0; i < 4; i++) {
      float splitDist = cascadeSplits[i];
      glm::vec3 frustumCorners[8] = {
          {-1.0f, 1.0f, -1.0f},  {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
          {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
          {1.0f, -1.0f, 1.0f},   {-1.0f, -1.0f, 1.0f}};

      glm::mat4 invCam = glm::inverse(m_camera.getProjectionMatrix() *
                                      m_camera.getViewMatrix());
      for (int j = 0; j < 8; j++) {
        glm::vec4 pt = invCam * glm::vec4(frustumCorners[j], 1.0f);
        frustumCorners[j] = glm::vec3(pt) / pt.w;
      }

      for (int j = 0; j < 4; j++) {
        glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
        frustumCorners[j + 4] =
            frustumCorners[j] + (dist * (splitDist / farClip));
        frustumCorners[j] =
            frustumCorners[j] + (dist * (lastSplitDist / farClip));
      }

      glm::vec3 center = glm::vec3(0.0f);
      for (int j = 0; j < 8; j++)
        center += frustumCorners[j];
      center /= 8.0f;

      float radius = 0.0f;
      for (int j = 0; j < 8; j++)
        radius = std::max(radius, glm::length(frustumCorners[j] - center));
      radius = std::ceil(radius * 16.0f) / 16.0f;

      glm::vec3 maxExtents = glm::vec3(radius);
      glm::vec3 minExtents = -maxExtents;

      glm::mat4 lightViewMatrix = glm::lookAt(
          center - lightDir * radius, center, glm::vec3(0.0f, 1.0f, 0.0f));
      glm::mat4 lightOrthoMatrix =
          glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y,
                     0.0f, 2.0f * radius);

      // Snap to texel
      glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
      glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      shadowOrigin *= 4096.0f / 2.0f; // ShadowMapSize = 4096
      glm::vec2 roundedOrigin = glm::round(glm::vec2(shadowOrigin));
      glm::vec2 roundOffset =
          (roundedOrigin - glm::vec2(shadowOrigin)) * 2.0f / 4096.0f;

      lightOrthoMatrix[3][0] += roundOffset.x;
      lightOrthoMatrix[3][1] += roundOffset.y;
      lightOrthoMatrix[1][1] *= -1;

      sd.cascadeViewProj[i] = lightOrthoMatrix * lightViewMatrix;
      lastSplitDist = splitDist;
    }

    sd.lightCount = (int)lights.size();
    sd.lightBufferIndex = m_sceneManager->getLightBufferIndex(m_currentFrame);
    sd.headlampEnabled = m_uiParams.enableHeadlamp ? 1 : 0;
    sd.visualizeCascades = m_uiParams.visualizeCascades ? 1 : 0;
    sd.shadowBias = m_uiParams.shadowBias;
    sd.shadowNormalBias = m_uiParams.shadowNormalBias;
    sd.pcfRange = m_uiParams.pcfRange;
    sd.csmLambda = m_uiParams.csmLambda;
    sd.irradianceIndex = m_envManager->getIrradianceIndex();
    sd.prefilteredIndex = m_envManager->getPrefilteredIndex();
    sd.brdfLutIndex = m_envManager->getBrdfLutIndex();
    // map index and others are filled by RendererSystem when setting up
    // resources? Actually sceneData expects binding indices. RendererSystem
    // should expose the indices it registered. We'll update the remaining
    // indices inside RendererSystem::render or just use getters. For now let's
    // set them here assuming we can get them. But Application doesn't know
    // about `shadowMapIndex`. We will pass `sd` to `renderer->render(...)` and
    // let it fill the resource indices before uploading.

    // Cluster grid dimensions - MUST match RendererSystem::initializePipelines
    sd.gridX = 16;
    sd.gridY = 9;
    sd.gridZ = 24;
    
    sd.nearClip = m_camera.getNear();
    sd.farClip = m_camera.getFar();
    sd.screenWidth = (float)m_window->getWidth();
    sd.screenHeight = (float)m_window->getHeight();

    m_sync->waitForFrame(m_currentFrame);

    // Update Buffers
    m_sceneManager->updateLightsBuffer(m_currentFrame);
    // sceneData update is finalized in renderer? No, we need to upload it.
    // The issue is `shadowMapIndex` etc. are in Renderer.
    // Let's defer `updateSceneData` call to inside `renderer.render`?
    // Or better, let `Renderer` fill the missing pieces of `sd` and then upload
    // it.

    // Render
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        m_context->getDevice(), m_swapchain->getHandle(), UINT64_MAX,
        m_sync->getImageAvailableSemaphore(m_currentFrame), VK_NULL_HANDLE,
        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      // Handle resize
      continue; // or onResize
    }

    m_sync->resetFence(m_currentFrame);

    auto &cmd = m_commandBuffers[m_currentFrame];
    cmd->begin();

    // Clear instances
    m_sceneManager->clearMeshInstances(m_currentFrame);
    // Re-add instances
    if (m_model) {
      for (const auto &mesh : m_model->meshes) {
        for (const auto &primitive : mesh.primitives) {
          m_sceneManager->addMeshInstance(
              m_currentFrame, glm::mat4(1.0f), primitive.materialIndex,
              primitive.indexCount, primitive.firstIndex, 0,
              primitive.boundingCenter, primitive.boundingRadius);
        }
      }
    }

    // DEBUG: Log mesh instance count
    spdlog::debug("Frame {}: Mesh instances: {}", m_currentFrame, 
                  m_sceneManager->getMeshInstanceCount(m_currentFrame));

    m_renderer->render(*cmd.get(), graph, *m_sceneManager.get(), m_currentFrame,
                       imageIndex, sd, m_swapchain.get(), m_sync.get(),
                       m_uiParams, m_model.get(), m_envManager->getSkyboxIndex());
    
    // Inject UI Pass (Overlay)
    // Depends on whatever the last pass wrote to "Swapchain".
    // We don't clear outputs because we draw on top.
    
    graph.addPass("UIPass", {}, {"Swapchain"}, [this](VkCommandBuffer cb){
        fprintf(stderr, "Application::run UIPass Lambda: Calling render. m_uiManager=%p\n", m_uiManager.get());
        m_uiManager->render(cb);
        fprintf(stderr, "Application::run UIPass Lambda: Returned from render.\n");
    }, false); // clearOutputs = false
    

    VkExtent2D ext = m_swapchain->getExtent();
    graph.execute(cmd->getHandle(), ext);

    cmd->end();

    // Submit
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores =
        &m_sync->getImageAvailableSemaphore(m_currentFrame);
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    VkCommandBuffer buffer = cmd->getHandle();
    submitInfo.pCommandBuffers = &buffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_imageSemaphores[imageIndex];

    if (vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo,
                      m_sync->getInFlightFence(m_currentFrame)) != VK_SUCCESS) {
      throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_imageSemaphores[imageIndex];
    VkSwapchainKHR swapChains[] = {m_swapchain->getHandle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);

    m_currentFrame = (m_currentFrame + 1) % 2;
  }

  vkDeviceWaitIdle(m_context->getDevice());
}

void Application::handleInput(float deltaTime) {
  if (glfwGetKey(m_window->getNativeWindow(), GLFW_KEY_W) == GLFW_PRESS)
    m_camera.processKeyboard(GLFW_KEY_W, true);
  else
    m_camera.processKeyboard(GLFW_KEY_W, false);
  if (glfwGetKey(m_window->getNativeWindow(), GLFW_KEY_S) == GLFW_PRESS)
    m_camera.processKeyboard(GLFW_KEY_S, true);
  else
    m_camera.processKeyboard(GLFW_KEY_S, false);
  if (glfwGetKey(m_window->getNativeWindow(), GLFW_KEY_A) == GLFW_PRESS)
    m_camera.processKeyboard(GLFW_KEY_A, true);
  else
    m_camera.processKeyboard(GLFW_KEY_A, false);
  if (glfwGetKey(m_window->getNativeWindow(), GLFW_KEY_D) == GLFW_PRESS)
    m_camera.processKeyboard(GLFW_KEY_D, true);
  else
    m_camera.processKeyboard(GLFW_KEY_D, false);
  if (glfwGetKey(m_window->getNativeWindow(), GLFW_KEY_Q) == GLFW_PRESS)
    m_camera.processKeyboard(GLFW_KEY_Q, true);
  else
    m_camera.processKeyboard(GLFW_KEY_Q, false);
  if (glfwGetKey(m_window->getNativeWindow(), GLFW_KEY_E) == GLFW_PRESS)
    m_camera.processKeyboard(GLFW_KEY_E, true);
  else
    m_camera.processKeyboard(GLFW_KEY_E, false);

  m_camera.update(deltaTime);
}

void Application::updateUI(float deltaTime) {
  m_uiManager->beginFrame();

  ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
  ImGui::Begin("Renderer Controls", nullptr, ImGuiWindowFlags_NoScrollbar);

  if (ImGui::BeginTabBar("RendererTabs")) {
    if (ImGui::BeginTabItem("Main")) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Performance");
      ImGui::Text("FPS: %.1f (%.3f ms)", 1.0f / deltaTime, deltaTime * 1000.0f);
      ImGui::Separator();

      ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Camera & Tonemaping");
      ImGui::DragFloat("Exposure", &m_uiParams.exposure, 0.01f, 0.0f, 10.0f);
      ImGui::DragFloat("Gamma", &m_uiParams.gamma, 0.01f, 0.5f, 5.0f);
      ImGui::DragFloat("IBL Intensity", &m_uiParams.iblIntensity, 0.01f, 0.0f, 5.0f);
      
      ImGui::Separator();
      ImGui::Checkbox("Show Skybox", &m_uiParams.showSkybox);
      ImGui::Checkbox("Enable Headlamp", &m_uiParams.enableHeadlamp);

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Post-Process")) {
      if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
        static bool bloomEnabled = true; // Temporary toggle or we could add to UIParams
        ImGui::Checkbox("Enable Bloom", &bloomEnabled); 
        ImGui::DragFloat("Strength", &m_uiParams.bloomStrength, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Threshold", &m_uiParams.bloomThreshold, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("Softness", &m_uiParams.bloomSoftness, 0.01f, 0.0f, 1.0f);
      }

      if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable SSAO", &m_uiParams.enableSSAO);
        ImGui::DragFloat("Radius", &m_uiParams.ssaoRadius, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Bias", &m_uiParams.ssaoBias, 0.001f, 0.0f, 0.1f);
      }

      if (ImGui::CollapsingHeader("Anti-Aliasing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable FXAA", &m_uiParams.enableFXAA);
      }

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Shadows")) {
      ImGui::Checkbox("Visualize CSM Cascades", &m_uiParams.visualizeCascades);
      ImGui::DragFloat("Shadow Bias", &m_uiParams.shadowBias, 0.0001f, 0.0f, 0.05f, "%.4f");
      ImGui::DragFloat("Normal Bias", &m_uiParams.shadowNormalBias, 0.0001f, 0.0f, 0.05f, "%.4f");
      ImGui::SliderInt("PCF Range", &m_uiParams.pcfRange, 0, 4);
      ImGui::SliderFloat("CSM Lambda", &m_uiParams.csmLambda, 0.0f, 1.0f);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Scene Inspector")) {
      if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& lights = m_sceneManager->getLights();
        std::vector<std::string> lightNames;
        for (size_t i = 0; i < lights.size(); ++i) {
          lightNames.push_back("Light " + std::to_string(i) + (i == 0 ? " (Sun)" : ""));
        }

        if (ImGui::BeginCombo("Select Light", lightNames[m_uiParams.selectedLight].c_str())) {
          for (int i = 0; i < (int)lightNames.size(); ++i) {
            bool isSelected = (m_uiParams.selectedLight == i);
            if (ImGui::Selectable(lightNames[i].c_str(), isSelected)) {
              m_uiParams.selectedLight = i;
            }
          }
          ImGui::EndCombo();
        }

        if (m_uiParams.selectedLight < (int)lights.size()) {
          Light& light = const_cast<Light&>(lights[m_uiParams.selectedLight]);
          ImGui::PushID("LightEditor");
          
          ImGui::Text("Type: %s", light.position.w == 1.0f ? "Directional" : (light.position.w == 0.0f ? "Point" : "Spot"));
          
          if (light.position.w == 1.0f) {
             float dir[3] = {light.direction.x, light.direction.y, light.direction.z};
             if (ImGui::DragFloat3("Direction", dir, 0.01f)) {
               light.direction = glm::vec4(glm::normalize(glm::vec3(dir[0], dir[1], dir[2])), light.direction.w);
             }
          } else {
             float pos[3] = {light.position.x, light.position.y, light.position.z};
             if (ImGui::DragFloat3("Position", pos, 0.1f)) {
               light.position = glm::vec4(pos[0], pos[1], pos[2], light.position.w);
             }
          }

          float color[3] = {light.color.r, light.color.g, light.color.b};
          if (ImGui::ColorEdit3("Color", color)) {
            light.color = glm::vec4(color[0], color[1], color[2], light.color.a);
          }
          ImGui::DragFloat("Intensity", &light.color.a, 0.1f, 0.0f, 100.0f);
          
          if (light.position.w != 1.0f) {
            ImGui::DragFloat("Range", &light.direction.w, 0.1f, 0.0f, 100.0f);
          }

          m_sceneManager->updateLight(m_uiParams.selectedLight, light);
          ImGui::PopID();
        }
      }

      ImGui::Separator();

      if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& materials = m_sceneManager->getMaterials();
        std::vector<std::string> matNames;
        for (size_t i = 0; i < materials.size(); ++i) {
          matNames.push_back("Material " + std::to_string(i));
        }

        if (ImGui::BeginCombo("Select Material", matNames[m_uiParams.selectedMaterial].c_str())) {
          for (int i = 0; i < (int)matNames.size(); ++i) {
            bool isSelected = (m_uiParams.selectedMaterial == i);
            if (ImGui::Selectable(matNames[i].c_str(), isSelected)) {
              m_uiParams.selectedMaterial = i;
            }
          }
          ImGui::EndCombo();
        }

        if (m_uiParams.selectedMaterial < (int)materials.size()) {
          MaterialMetadata& mat = const_cast<MaterialMetadata&>(materials[m_uiParams.selectedMaterial]);
          ImGui::PushID("MaterialEditor");
          
          float baseColor[4] = {mat.baseColorFactor.r, mat.baseColorFactor.g, mat.baseColorFactor.b, mat.baseColorFactor.a};
          if (ImGui::ColorEdit4("Base Color", baseColor)) {
            mat.baseColorFactor = glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
          }

          ImGui::SliderFloat("Metallic", &mat.metallicFactor, 0.0f, 1.0f);
          ImGui::SliderFloat("Roughness", &mat.roughnessFactor, 0.0f, 1.0f);
          ImGui::DragFloat("Alpha Cutoff", &mat.alphaCutoff, 0.01f, 0.0f, 1.0f);

          m_sceneManager->updateMaterial(m_uiParams.selectedMaterial, mat);
          ImGui::PopID();
        }
      }

      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::End();
  m_uiManager->endFrame();
}

void Application::cleanup() {
  vkDeviceWaitIdle(m_context->getDevice());
  for (auto &sem : m_imageSemaphores) {
    vkDestroySemaphore(m_context->getDevice(), sem, nullptr);
  }
}

} // namespace astral
