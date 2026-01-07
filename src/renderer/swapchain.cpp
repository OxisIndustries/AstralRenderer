#include "astral/renderer/swapchain.hpp"
#include "astral/core/context.hpp"
#include "astral/platform/window.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace astral {

Swapchain::Swapchain(Context *context, Window *window)
    : m_context(context), m_window(window) {
  create();
  createImageViews();
}

Swapchain::~Swapchain() {
  for (auto imageView : m_imageViews) {
    vkDestroyImageView(m_context->getDevice(), imageView, nullptr);
  }
  vkDestroySwapchainKHR(m_context->getDevice(), m_swapchain, nullptr);
}

void Swapchain::create() {
  SwapchainSupportDetails support =
      querySupport(m_context->getPhysicalDevice());

  VkSurfaceFormatKHR surfaceFormat = chooseFormat(support.formats);
  VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
  VkExtent2D extent = chooseExtent(support.capabilities, m_window);

  uint32_t imageCount = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 &&
      imageCount > support.capabilities.maxImageCount) {
    imageCount = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_context->getSurface();
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = m_context->getQueueFamilyIndices();
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = support.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_context->getDevice(), &createInfo, nullptr,
                           &m_swapchain) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create swapchain!");
  }

  vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapchain, &imageCount,
                          nullptr);
  m_images.resize(imageCount);
  vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapchain, &imageCount,
                          m_images.data());

  m_imageFormat = surfaceFormat.format;
  m_extent = extent;

  spdlog::info("Swapchain created: {}x{}, Images: {}", m_extent.width,
               m_extent.height, m_images.size());
}

void Swapchain::createImageViews() {
  m_imageViews.resize(m_images.size());

  for (size_t i = 0; i < m_images.size(); i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = m_images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m_imageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_context->getDevice(), &createInfo, nullptr,
                          &m_imageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create image views!");
    }
  }
}

SwapchainSupportDetails Swapchain::querySupport(VkPhysicalDevice device) {
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_context->getSurface(),
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_context->getSurface(),
                                       &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_context->getSurface(),
                                         &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_context->getSurface(),
                                            &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_context->getSurface(),
                                              &presentModeCount,
                                              details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR Swapchain::chooseFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

VkPresentModeKHR Swapchain::choosePresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  // Force FIFO for validation stability (prevents semaphore signaling issues
  // with Mailbox discarded frames)
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                   Window *window) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D actualExtent = {window->getWidth(), window->getHeight()};
    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return actualExtent;
  }
}

} // namespace astral
