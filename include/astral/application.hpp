#pragma once

#include "astral/astral.hpp"
#include "astral/core/commands.hpp"
#include "astral/core/context.hpp"
#include "astral/core/window.hpp"
#include "astral/renderer/camera.hpp"
#include "astral/renderer/environment_manager.hpp"
#include "astral/renderer/gltf_loader.hpp"
#include "astral/renderer/renderer_system.hpp"
#include "astral/renderer/scene_manager.hpp"
#include "astral/renderer/swapchain.hpp"
#include "astral/renderer/sync.hpp"
#include "astral/renderer/ui_manager.hpp"

#include <memory>
#include <vector>

namespace astral {

// UIParams is defined in renderer_system.hpp

class Application {
public:
  Application();
  ~Application();

  void run();

private:
  void init();
  void cleanup();
  void initScene();
  void handleInput(float deltaTime);
  void updateUI(float deltaTime);

  // Core
  std::unique_ptr<Window> m_window;
  std::unique_ptr<Context> m_context;
  std::unique_ptr<Swapchain> m_swapchain;
  std::unique_ptr<FrameSync> m_sync;
  std::unique_ptr<CommandPool> m_commandPool;
  std::vector<std::unique_ptr<CommandBuffer>> m_commandBuffers;
  std::vector<VkSemaphore> m_imageSemaphores;

  // Managers
  std::unique_ptr<SceneManager> m_sceneManager;
  std::unique_ptr<EnvironmentManager> m_envManager;
  std::unique_ptr<UIManager> m_uiManager;
  std::unique_ptr<GltfLoader> m_loader;

  // Renderer
  std::unique_ptr<RendererSystem> m_renderer;

  // Scene
  Camera m_camera;
  std::shared_ptr<Model> m_model;
  UIParams m_uiParams;

  // State
  uint32_t m_currentFrame = 0;
  float m_lastFrameTime = 0.0f;
  bool m_firstFrame = true;
  SceneData m_prevSceneData = {};
  uint32_t m_frameIndex = 0;
};

} // namespace astral
