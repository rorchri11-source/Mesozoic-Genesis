#pragma once
#include "UberMesh.h"
#include "Window.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#ifndef VULKAN_SDK_AVAILABLE
#define VULKAN_SDK_AVAILABLE 1
#endif
#else
#ifndef VULKAN_SDK_AVAILABLE
#define VULKAN_SDK_AVAILABLE 0
#endif
// Dummy types if SDK headers are missing
typedef void *VkInstance;
typedef void *VkPhysicalDevice;
typedef void *VkDevice;
typedef void *VkQueue;
typedef void *VkSurfaceKHR;
typedef void *VkSwapchainKHR;
typedef void *VkRenderPass;
typedef void *VkCommandPool;
typedef void *VkCommandBuffer;
typedef void *VkSemaphore;
typedef void *VkFence;
typedef void *VkFramebuffer;
typedef void *VkImageView;
typedef void *VkImage;
typedef void *VkBuffer;
typedef void *VkDeviceMemory;
typedef void *VkDescriptorSetLayout;
typedef void *VkDescriptorPool;
typedef void *VkDescriptorSet;
typedef void *VkShaderModule;
typedef void *VkPipelineLayout;
typedef void *VkPipeline;
typedef uint32_t VkFormat;
typedef uint32_t VkResult;
typedef uint32_t VkFlags;
typedef uint32_t VkMemoryPropertyFlags;
typedef uint32_t VkBufferUsageFlags;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkShaderStageFlags;

#define VK_NULL_HANDLE nullptr
#define VK_SUCCESS 0
// Constants needed for SwapchainConfig default values
#define VK_FORMAT_B8G8R8A8_SRGB 44
#define VK_FORMAT_D32_SFLOAT 126
#define VK_SHADER_STAGE_VERTEX_BIT 0x00000001
#define VK_SHADER_STAGE_FRAGMENT_BIT 0x00000010
#endif

namespace Mesozoic {
namespace Graphics {

struct VertexInputDescription {
#if VULKAN_SDK_AVAILABLE
  static VkVertexInputBindingDescription GetBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 6>
  GetAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions{};

    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    // UV
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, uv);

    // Normal
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, normal);

    // Tangent
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, tangent);

    // Bone Indices
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_UINT;
    attributeDescriptions[4].offset = offsetof(Vertex, boneIndices);

    // Bone Weights
    attributeDescriptions[5].binding = 0;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(Vertex, boneWeights);

    return attributeDescriptions;
  }
#endif
};

struct GPUBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  size_t size = 0;
  void *mapped = nullptr;
  bool IsValid() const { return buffer != VK_NULL_HANDLE; }
};

struct GPUMesh {
  GPUBuffer vertexBuffer;
  GPUBuffer indexBuffer;
  uint32_t indexCount;
};

struct GPUTexture {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  uint32_t width = 0;
  uint32_t height = 0;
  VkFormat format = VK_FORMAT_UNDEFINED;
  bool IsValid() const { return image != VK_NULL_HANDLE; }
};

struct SwapchainConfig {
  uint32_t width = 1920;
  uint32_t height = 1080;
  uint32_t imageCount = 2;
  VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
  bool vsync = true;
};

struct SwapchainData {
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
  std::vector<VkFramebuffer> framebuffers;
  SwapchainConfig config;
  VkImage depthImage = VK_NULL_HANDLE;
  VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
  VkImageView depthImageView = VK_NULL_HANDLE;
};

enum class RenderPassType : uint8_t {
  ShadowMap,
  GBuffer,
  Lighting,
  SSS,
  PostProcess,
  UI,
  COUNT
};

struct RenderPassData {
  RenderPassType type;
  VkRenderPass renderPass = VK_NULL_HANDLE;
};

struct FrameData {
  VkSemaphore imageAvailable = VK_NULL_HANDLE;
  // VkSemaphore renderFinished = VK_NULL_HANDLE; // Moved to per-image
  VkFence inFlight = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

class VulkanBackend {
public:
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  SwapchainData swapchain;
  std::vector<RenderPassData> renderPasses;
  static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> frames;
  std::vector<VkFence> imagesInFlight; // Tracks if a swapchain image is in use
  std::vector<VkSemaphore> renderFinishedSemaphores; // One per swapchain image
  uint32_t currentFrame = 0;
  bool initialized = false;

  bool CreateDescriptorSetLayout() {
#if VULKAN_SDK_AVAILABLE
    VkDescriptorSetLayoutBinding heightMapBinding{};
    heightMapBinding.binding = 0;
    heightMapBinding.descriptorCount = 1;
    heightMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    heightMapBinding.pImmutableSamplers = nullptr;
    heightMapBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding splatMapBinding{};
    splatMapBinding.binding = 1;
    splatMapBinding.descriptorCount = 1;
    splatMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    splatMapBinding.pImmutableSamplers = nullptr;
    splatMapBinding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {heightMapBinding,
                                                            splatMapBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &descriptorSetLayout) != VK_SUCCESS) {
      return false;
    }
    return true;
#else
    return true;
#endif
  }

  bool CreateDescriptorPool() {
#if VULKAN_SDK_AVAILABLE
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 100;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 10;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) !=
        VK_SUCCESS) {
      return false;
    }
    return true;
#else
    return true;
#endif
  }

  bool CreateDescriptorSets() {
#if VULKAN_SDK_AVAILABLE
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) !=
        VK_SUCCESS) {
      return false;
    }
    return true;
#else
    return true;
#endif
  }

  void UpdateDescriptorSets(GPUTexture &heightMap, GPUTexture &splatMap) {
#if VULKAN_SDK_AVAILABLE
    VkDescriptorImageInfo heightInfo{};
    heightInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    heightInfo.imageView = heightMap.view;
    heightInfo.sampler = heightMap.sampler;

    VkDescriptorImageInfo splatInfo{};
    splatInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    splatInfo.imageView = splatMap.view;
    splatInfo.sampler = splatMap.sampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &heightInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &splatInfo;

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
#endif
  }

  bool Initialize(Window &window) {
    if (!VULKAN_SDK_AVAILABLE)
      return false;
    try {
      if (!CreateInstance(window))
        return false;
      if (!CreateSurface(window))
        return false;
      if (!PickPhysicalDevice())
        return false;
      if (!CreateLogicalDevice())
        return false;
      if (!CreateSwapchain(window))
        return false;
      if (!CreateSyncObjects())
        return false;
      if (!CreateCommandBuffers())
        return false;

      if (!CreateSimpleRenderPass())
        return false;

      if (!CreateDescriptorSetLayout())
        return false;

      if (!CreateGraphicsPipeline()) {
        std::cerr << "[Vulkan] Failed to create graphics pipeline!"
                  << std::endl;
        return false;
      }

      if (!CreateDescriptorPool())
        return false;
      if (!CreateDescriptorSets())
        return false;

      if (!CreateFramebuffers())
        return false;

      imagesInFlight.resize(swapchain.images.size(), VK_NULL_HANDLE);

      initialized = true;
      std::cout << "[Vulkan] Initialized successfully (Win32)!" << std::endl;
      return true;
    } catch (const std::exception &e) {
      std::cerr << "[Vulkan] Exception: " << e.what() << std::endl;
      return false;
    }
  }

  void BindTerrainTextures() {
#if VULKAN_SDK_AVAILABLE
    if (descriptorSet != VK_NULL_HANDLE) {
      vkCmdBindDescriptorSets(frames[currentFrame].commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                              0, 1, &descriptorSet, 0, nullptr);
    }
#endif
  }

  void WaitIdle() {
#if VULKAN_SDK_AVAILABLE
    if (device)
      vkDeviceWaitIdle(device);
#endif
  }

  bool BeginFrame(uint32_t &imageIndex) {
    if (!initialized)
      return false;
#if VULKAN_SDK_AVAILABLE
    vkWaitForFences(device, 1, &frames[currentFrame].inFlight, VK_TRUE,
                    UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        device, swapchain.swapchain, UINT64_MAX,
        frames[currentFrame].imageAvailable, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
      return false;
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
      return false;

    // Check if a previous frame is using this image (wait for its fence)
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
      vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE,
                      UINT64_MAX);
    }
    // Mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = frames[currentFrame].inFlight;

    vkResetFences(device, 1, &frames[currentFrame].inFlight);
    vkResetCommandBuffer(frames[currentFrame].commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(frames[currentFrame].commandBuffer, &beginInfo);
#endif
    return true;
  }

  void BeginRenderPass(RenderPassType type, uint32_t imageIndex) {
    (void)type;
#if VULKAN_SDK_AVAILABLE
    if (imageIndex >= swapchain.framebuffers.size())
      return;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPasses[0].renderPass;
    renderPassInfo.framebuffer = swapchain.framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {swapchain.config.width,
                                        swapchain.config.height};

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.4f, 0.6f, 0.9f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(frames[currentFrame].commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
#endif
  }

  void EndRenderPass() {
#if VULKAN_SDK_AVAILABLE
    vkCmdEndRenderPass(frames[currentFrame].commandBuffer);
#endif
  }

  void EndFrame(uint32_t imageIndex) {
#if VULKAN_SDK_AVAILABLE
    vkEndCommandBuffer(frames[currentFrame].commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {frames[currentFrame].imageAvailable};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frames[currentFrame].commandBuffer;

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, frames[currentFrame].inFlight);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
#endif
  }

  void Cleanup() {
#if VULKAN_SDK_AVAILABLE
    if (device)
      vkDeviceWaitIdle(device);

    if (graphicsPipeline)
      vkDestroyPipeline(device, graphicsPipeline, nullptr);
    if (pipelineLayout)
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      if (frames[i].imageAvailable)
        vkDestroySemaphore(device, frames[i].imageAvailable, nullptr);
      // if (frames[i].renderFinished)
      //     vkDestroySemaphore(device, frames[i].renderFinished, nullptr);
      if (frames[i].inFlight)
        vkDestroyFence(device, frames[i].inFlight, nullptr);
      if (frames[i].commandPool)
        vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
    }

    for (auto semaphore : renderFinishedSemaphores) {
      vkDestroySemaphore(device, semaphore, nullptr);
    }

    for (auto fb : swapchain.framebuffers)
      vkDestroyFramebuffer(device, fb, nullptr);
    for (auto iv : swapchain.imageViews)
      vkDestroyImageView(device, iv, nullptr);
    if (swapchain.swapchain)
      vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);

    for (auto &rp : renderPasses)
      vkDestroyRenderPass(device, rp.renderPass, nullptr);

    if (swapchain.depthImageView)
      vkDestroyImageView(device, swapchain.depthImageView, nullptr);
    if (swapchain.depthImage)
      vkDestroyImage(device, swapchain.depthImage, nullptr);
    if (swapchain.depthImageMemory)
      vkFreeMemory(device, swapchain.depthImageMemory, nullptr);

    if (device)
      vkDestroyDevice(device, nullptr);
    if (surface)
      vkDestroySurfaceKHR(instance, surface, nullptr);

    if (instance)
      vkDestroyInstance(instance, nullptr);
#endif
  }

  uint32_t FindMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties) {
#if VULKAN_SDK_AVAILABLE
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
              properties) {
        return i;
      }
    }
#endif
    return 0;
  }

  GPUBuffer CreateBuffer(size_t size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties) {
    GPUBuffer buf;
    buf.size = size;
#if VULKAN_SDK_AVAILABLE
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buf.buffer) !=
        VK_SUCCESS) {
      std::cerr << "[Vulkan] Failed to create buffer!" << std::endl;
      return buf;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buf.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex =
        FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &buf.memory) !=
        VK_SUCCESS) {
      std::cerr << "[Vulkan] Failed to allocate buffer memory!" << std::endl;
      return buf;
    }

    if (vkBindBufferMemory(device, buf.buffer, buf.memory, 0) != VK_SUCCESS) {
      return buf;
    }

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vkMapMemory(device, buf.memory, 0, size, 0, &buf.mapped);
    }
#endif
    return buf;
  }

  void DestroyBuffer(GPUBuffer &buf) {
#if VULKAN_SDK_AVAILABLE
    if (buf.buffer)
      vkDestroyBuffer(device, buf.buffer, nullptr);
    if (buf.memory)
      vkFreeMemory(device, buf.memory, nullptr);
#endif
    buf.buffer = VK_NULL_HANDLE;
    buf.memory = VK_NULL_HANDLE;
    buf.mapped = nullptr;
  }

  void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
#if VULKAN_SDK_AVAILABLE
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool =
        frames[0].commandPool; // Use frame 0 pool for transfer
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, frames[0].commandPool, 1, &commandBuffer);
#endif
  }

  // --- TEXTURE HELPERS ---
  GPUTexture CreateTextureFromBuffer(void *data, size_t size, uint32_t width,
                                     uint32_t height, VkFormat format) {
    GPUTexture tex;
#if VULKAN_SDK_AVAILABLE
    tex.width = width;
    tex.height = height;
    tex.format = format;

    // 1. Staging Buffer
    GPUBuffer staging = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (staging.mapped) {
      memcpy(staging.mapped, data, size);
    }

    // 2. Create Image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &tex.image) != VK_SUCCESS) {
      std::cerr << "Failed to create image!" << std::endl;
      return tex;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, tex.image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &tex.memory) !=
        VK_SUCCESS) {
      std::cerr << "Failed to allocate image memory!" << std::endl;
      return tex;
    }

    vkBindImageMemory(device, tex.image, tex.memory, 0);

    // 3. Transition & Copy (using helpers)
    TransitionImageLayout(tex.image, format, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(staging.buffer, tex.image, width, height);
    TransitionImageLayout(tex.image, format,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    DestroyBuffer(staging);

    // 4. Create View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &tex.view) !=
        VK_SUCCESS) {
      std::cerr << "Failed to create image view!" << std::endl;
      return tex;
    }

    // 5. Create Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &tex.sampler) !=
        VK_SUCCESS) {
      std::cerr << "Failed to create sampler!" << std::endl;
      return tex;
    }
#endif
    return tex;
  }

private:
  VkCommandBuffer BeginSingleTimeCommands() {
#if VULKAN_SDK_AVAILABLE
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = frames[0].commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
#else
    return nullptr;
#endif
  }

  void EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
#if VULKAN_SDK_AVAILABLE
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, frames[0].commandPool, 1, &commandBuffer);
#endif
  }

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout) {
#if VULKAN_SDK_AVAILABLE
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(commandBuffer);
#endif
  }

  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height) {
#if VULKAN_SDK_AVAILABLE
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(commandBuffer);
#endif
  }

  // NEW: Update existing texture with new data (for Terrain Painting)
  void UpdateTexture(GPUTexture &texture, void *data, size_t size) {
#if VULKAN_SDK_AVAILABLE
    if (!texture.IsValid())
      return;

    // 1. Create Staging Buffer
    GPUBuffer staging = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // 2. Map & Copy
    void *mapped;
    vkMapMemory(device, staging.memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, staging.memory);

    // 3. Transition to Transfer Dst
    TransitionImageLayout(texture.image, texture.format,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Copy Buffer to Image
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {texture.width, texture.height, 1};
    vkCmdCopyBufferToImage(commandBuffer, staging.buffer, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    EndSingleTimeCommands(commandBuffer);

    // 5. Transition back to Shader Read
    TransitionImageLayout(texture.image, texture.format,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // 6. Cleanup
    DestroyBuffer(staging);
#endif
  }
#if VULKAN_SDK_AVAILABLE
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

  std::vector<const char *> extensions = window.GetRequiredVulkanExtensions();
  // Add debug utils if needed, or rely on implicit layer extensions?
  // Usually surface extensions are enough.
  // Win32 needs VK_KHR_win32_surface

  createInfo.enabledExtensionCount = (uint32_t)extensions.size();
  createInfo.ppEnabledExtensionNames = extensions.data();

  const char *validationLayer = "VK_LAYER_KHRONOS_validation";
  createInfo.enabledLayerCount = 1;
  createInfo.ppEnabledLayerNames = &validationLayer;
  return vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS;
#else
  return true;
#endif
}

bool CreateSurface(Window &window) {
#if VULKAN_SDK_AVAILABLE
  VkWin32SurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hinstance = window.hInstance;
  createInfo.hwnd = window.handle;
  if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) !=
      VK_SUCCESS) {
    std::cerr << "[Vulkan] Failed to create Win32 surface!" << std::endl;
    return false;
  }
  return true;
#else
  return true;
#endif
}

bool PickPhysicalDevice() { /* Assumed simple selection */ return true; }

bool CreateLogicalDevice() {
#if VULKAN_SDK_AVAILABLE
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount == 0)
    return false;
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
  physicalDevice = devices[0];

  float qp = 1.0f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = 0; // Check specific family support if needed
  qci.queueCount = 1;
  qci.pQueuePriorities = &qp;

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;

  const char *ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  dci.enabledExtensionCount = 1;
  dci.ppEnabledExtensionNames = &ext;

  if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS)
    return false;
  vkGetDeviceQueue(device, 0, 0, &graphicsQueue);
  presentQueue = graphicsQueue;
#endif
  return true;
}

bool CreateSwapchain(Window &w) {
#if VULKAN_SDK_AVAILABLE
  VkSwapchainCreateInfoKHR sci{};
  sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  sci.surface = surface;
  sci.minImageCount = 2;
  sci.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  sci.imageExtent = {w.config.width, w.config.height};
  sci.imageArrayLayers = 1;
  sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  sci.clipped = VK_TRUE;
  sci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
  if (caps.currentExtent.width != 0xFFFFFFFF) {
    sci.imageExtent = caps.currentExtent;
  }

  if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain.swapchain) !=
      VK_SUCCESS)
    return false;

  uint32_t imgCount;
  vkGetSwapchainImagesKHR(device, swapchain.swapchain, &imgCount, nullptr);
  swapchain.images.resize(imgCount);
  vkGetSwapchainImagesKHR(device, swapchain.swapchain, &imgCount,
                          swapchain.images.data());

  swapchain.config.width = sci.imageExtent.width;
  swapchain.config.height = sci.imageExtent.height;

  swapchain.imageViews.resize(imgCount);
  for (size_t i = 0; i < imgCount; i++) {
    VkImageViewCreateInfo ivci{};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = swapchain.images[i];
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_B8G8R8A8_SRGB;
    ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &ivci, nullptr, &swapchain.imageViews[i]) !=
        VK_SUCCESS)
      return false;
  }

  // Create Depth Image
  VkImageCreateInfo depthImgInfo{};
  depthImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
  depthImgInfo.extent.width = swapchain.config.width;
  depthImgInfo.extent.height = swapchain.config.height;
  depthImgInfo.extent.depth = 1;
  depthImgInfo.mipLevels = 1;
  depthImgInfo.arrayLayers = 1;
  depthImgInfo.format = VK_FORMAT_D32_SFLOAT;
  depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthImgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  depthImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vkCreateImage(device, &depthImgInfo, nullptr, &swapchain.depthImage);

  VkMemoryRequirements memReq;
  vkGetImageMemoryRequirements(device, swapchain.depthImage, &memReq);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = FindMemoryType(
      memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vkAllocateMemory(device, &allocInfo, nullptr, &swapchain.depthImageMemory);
  vkBindImageMemory(device, swapchain.depthImage, swapchain.depthImageMemory,
                    0);

  VkImageViewCreateInfo depthViewInfo{};
  depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depthViewInfo.image = swapchain.depthImage;
  depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
  depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depthViewInfo.subresourceRange.levelCount = 1;
  depthViewInfo.subresourceRange.layerCount = 1;

  vkCreateImageView(device, &depthViewInfo, nullptr, &swapchain.depthImageView);
#endif
  return true;
}

bool CreateSimpleRenderPass() {
#if VULKAN_SDK_AVAILABLE
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = VK_FORMAT_D32_SFLOAT;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  std::array<VkAttachmentDescription, 2> attachments = {colorAttachment,
                                                        depthAttachment};

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  RenderPassData rp;
  if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &rp.renderPass) !=
      VK_SUCCESS)
    return false;
  renderPasses.clear();
  renderPasses.push_back(rp);
#endif
  return true;
}

bool CreateFramebuffers() {
#if VULKAN_SDK_AVAILABLE
  swapchain.framebuffers.resize(swapchain.imageViews.size());
  for (size_t i = 0; i < swapchain.imageViews.size(); i++) {
    std::array<VkImageView, 2> attachments = {swapchain.imageViews[i],
                                              swapchain.depthImageView};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPasses[0].renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapchain.config.width;
    framebufferInfo.height = swapchain.config.height;
    framebufferInfo.layers = 1;
    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                            &swapchain.framebuffers[i]) != VK_SUCCESS)
      return false;
  }
#endif
  return true;
}

public:
void BindPipeline(VkPipeline pipeline) {
#if VULKAN_SDK_AVAILABLE
  vkCmdBindPipeline(frames[currentFrame].commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
#endif
}

void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                   uint32_t offset, uint32_t size, const void *pValues) {
#if VULKAN_SDK_AVAILABLE
  vkCmdPushConstants(frames[currentFrame].commandBuffer, layout, stageFlags,
                     offset, size, pValues);
#endif
}

void DestroyMesh(GPUMesh &mesh) {
  DestroyBuffer(mesh.vertexBuffer);
  DestroyBuffer(mesh.indexBuffer);
}

void DrawMesh(const GPUMesh &mesh) {
#if VULKAN_SDK_AVAILABLE
  VkBuffer vertexBuffers[] = {mesh.vertexBuffer.buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(frames[currentFrame].commandBuffer, 0, 1,
                         vertexBuffers, offsets);

  if (mesh.indexBuffer.IsValid()) {
    vkCmdBindIndexBuffer(frames[currentFrame].commandBuffer,
                         mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(frames[currentFrame].commandBuffer, mesh.indexCount, 1, 0,
                     0, 0);
  } else {
    vkCmdDraw(frames[currentFrame].commandBuffer, mesh.indexCount, 1, 0, 0);
  }
#endif
}

void DrawMeshInstanced(const GPUMesh &mesh, uint32_t instanceCount) {
#if VULKAN_SDK_AVAILABLE
  VkBuffer vertexBuffers[] = {mesh.vertexBuffer.buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(frames[currentFrame].commandBuffer, 0, 1,
                         vertexBuffers, offsets);

  if (mesh.indexBuffer.IsValid()) {
    vkCmdBindIndexBuffer(frames[currentFrame].commandBuffer,
                         mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(frames[currentFrame].commandBuffer, mesh.indexCount,
                     instanceCount, 0, 0, 0);
  } else {
    vkCmdDraw(frames[currentFrame].commandBuffer, mesh.indexCount,
              instanceCount, 0, 0);
  }
#endif
}

// Template wrapper for any vertex type
template <typename T>
GPUMesh UploadMesh(const std::vector<T> &vertices,
                   const std::vector<uint32_t> &indices) {
  GPUMesh mesh = {};
  mesh.indexCount = (uint32_t)indices.size();
#if VULKAN_SDK_AVAILABLE
  if (!vertices.empty()) {
    VkDeviceSize bufferSize = sizeof(T) * vertices.size();

    GPUBuffer stagingBuffer =
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (stagingBuffer.mapped) {
      memcpy(stagingBuffer.mapped, vertices.data(), (size_t)bufferSize);
    }

    mesh.vertexBuffer = CreateBuffer(bufferSize,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    CopyBuffer(stagingBuffer.buffer, mesh.vertexBuffer.buffer, bufferSize);
    DestroyBuffer(stagingBuffer);
  }

  if (!indices.empty()) {
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    GPUBuffer stagingBuffer =
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (stagingBuffer.mapped) {
      memcpy(stagingBuffer.mapped, indices.data(), (size_t)bufferSize);
    }

    mesh.indexBuffer = CreateBuffer(bufferSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    CopyBuffer(stagingBuffer.buffer, mesh.indexBuffer.buffer, bufferSize);
    DestroyBuffer(stagingBuffer);
  }
#endif
  return mesh;
}

bool CreateCommandBuffers() {
#if VULKAN_SDK_AVAILABLE
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = 0;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateCommandPool(device, &poolInfo, nullptr,
                            &frames[i].commandPool) != VK_SUCCESS)
      return false;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = frames[i].commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &allocInfo, &frames[i].commandBuffer);
  }
#endif
  return true;
}

bool CreateSyncObjects() {
#if VULKAN_SDK_AVAILABLE
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                      &frames[i].imageAvailable);
    // vkCreateSemaphore(device, &semaphoreInfo, nullptr,
    // &frames[i].renderFinished); // Removed
    vkCreateFence(device, &fenceInfo, nullptr, &frames[i].inFlight);
  }

  // Create per-image semaphores for RenderFinished
  renderFinishedSemaphores.resize(swapchain.images.size());
  for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &renderFinishedSemaphores[i]) != VK_SUCCESS) {
      return false;
    }
  }
#endif
  return true;
}

static std::vector<char> ReadFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file: " + filename);
  }
  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  return buffer;
}

VkShaderModule CreateShaderModule(const std::vector<char> &code) {
#if VULKAN_SDK_AVAILABLE
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }
  return shaderModule;
#else
  return VK_NULL_HANDLE;
#endif
}

bool CreateGraphicsPipeline() {
#if VULKAN_SDK_AVAILABLE
  try {
    auto vertShaderCode = ReadFile("vert.spv");
    auto fragShaderCode = ReadFile("frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    auto bindingDescription = VertexInputDescription::GetBindingDescription();
    auto attributeDescriptions =
        VertexInputDescription::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain.config.width;
    viewport.height = (float)swapchain.config.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {swapchain.config.width, swapchain.config.height};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128; // Up to 128 bytes

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                               &pipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPasses[0].renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &graphicsPipeline) != VK_SUCCESS) {
      throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[Vulkan] Pipeline Creation Failed: " << e.what() << std::endl;
    return false;
  }
#else
  return true;
#endif
}
// NEW: Create UI Pipeline (No Depth Test, Alpha Blending)
bool CreateUIPipeline(const std::string &vertPath,
                      const std::string &fragPath) {
#if VULKAN_SDK_AVAILABLE
  try {
    auto vertShaderCode = ReadFile(vertPath);
    auto fragShaderCode = ReadFile(fragPath);

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Alpha Blending for UI
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // No Depth Test for UI Overlay
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount =
        static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128; // Up to 128 bytes

    // Reusing pipelineLayout for now as per previous logic
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPasses[0].renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &graphicsPipeline) != VK_SUCCESS) {
      throw std::runtime_error("failed to create UI pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[Vulkan] UI Pipeline Creation Failed: " << e.what()
              << std::endl;
    return false;
  }
#else
  return true;
#endif
}
VkCommandBuffer GetCommandBuffer(uint32_t imageIndex) {
#if VULKAN_SDK_AVAILABLE
  return frames[currentFrame].commandBuffer;
#else
  return nullptr;
#endif
}
};
