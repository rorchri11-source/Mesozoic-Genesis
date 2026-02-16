#include "UISystem.h"
#include <iostream>

namespace Mesozoic {
namespace Graphics {

void UISystem::Initialize(VulkanBackend *backend, Window *window) {
  this->backend = backend;
  this->window = window;
  CreateQuadMesh();
}

void UISystem::CreateQuadMesh() {
  // Defines a unit quad 0..1
  // We will use this for all UI elements, scaling it via Push Constants.

  // In a real engine, we'd upload this to a GPUBuffer.
  // For now, we are going to rely on the fact that we can't easily upload
  // without full access to the memory allocator in this snippet.
  // However, we DO have backend->CreateBuffer.

  // Vertices
  std::vector<Vertex> vertices(4);
  vertices[0].position = {0, 0, 0};
  vertices[0].uv = {0, 0};
  vertices[0].normal = {0, 0, 1};
  vertices[1].position = {1, 0, 0};
  vertices[1].uv = {1, 0};
  vertices[1].normal = {0, 0, 1};
  vertices[2].position = {1, 1, 0};
  vertices[2].uv = {1, 1};
  vertices[2].normal = {0, 0, 1};
  vertices[3].position = {0, 1, 0};
  vertices[3].uv = {0, 1};
  vertices[3].normal = {0, 0, 1};

  std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

  // Upload to GPU
  size_t vertSize = sizeof(Vertex) * vertices.size();
  size_t indSize = sizeof(uint32_t) * indices.size();

  // Create Staging
  GPUBuffer stageV =
      backend->CreateBuffer(vertSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *data;
  vkMapMemory(backend->device, stageV.memory, 0, vertSize, 0, &data);
  memcpy(data, vertices.data(), vertSize);
  vkUnmapMemory(backend->device, stageV.memory);

  // Create Device Local
  quadMesh.vertexBuffer = backend->CreateBuffer(
      vertSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  backend->CopyBuffer(stageV.buffer, quadMesh.vertexBuffer.buffer, vertSize);
  backend->DestroyBuffer(stageV);

  // Index Buffer
  GPUBuffer stageI =
      backend->CreateBuffer(indSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vkMapMemory(backend->device, stageI.memory, 0, indSize, 0, &data);
  memcpy(data, indices.data(), indSize);
  vkUnmapMemory(backend->device, stageI.memory);

  quadMesh.indexBuffer = backend->CreateBuffer(
      indSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  backend->CopyBuffer(stageI.buffer, quadMesh.indexBuffer.buffer, indSize);
  backend->DestroyBuffer(stageI);

  quadMesh.indexCount = (uint32_t)indices.size();
}

void UISystem::BeginFrame() {
  drawList.clear();

  // Update Projection (Ortho 0..W, 0..H)
  float w = (float)window->config.width;
  float h = (float)window->config.height;

  // Ortho: 0, w, 0, h (Top-Left Origin)
  projection = glm::ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f);
}

void UISystem::DrawImage(float x, float y, float w, float h,
                         GPUTexture &texture, glm::vec4 color) {
  if (!texture.IsValid())
    return;
  UIElement el;
  el.x = x;
  el.y = y;
  el.width = w;
  el.height = h;
  el.texture = &texture;
  el.color = color;
  el.isButton = false;
  drawList.push_back(el);
}

bool UISystem::DrawButton(float x, float y, float w, float h,
                          GPUTexture &texture, glm::vec4 color,
                          glm::vec4 hoverColor) {
  if (!texture.IsValid())
    return false;

  float mx, my;
  window->GetMousePosition(mx, my);

  // AABB Check
  bool hover = (mx >= x && mx <= x + w && my >= y && my <= y + h);
  bool down = window->IsMouseButtonDown(Window::MOUSE_LEFT);

  // Returns true if hovered and mouse is down.
  // Ideally we track "Clicked" (Down -> Up) but for painting tools "Down" is
  // often what we want. For buttons, we might want "Clicked". Let's return
  // logic for "Pressed" frame? No, let's return Down state and let app handle
  // one-shot. ACTUALLY: Standard Immediate Mode GUI 'Button' returns TRUE only
  // on CLICK (Release). But for simplicity here, we'll return TRUE while HELD
  // if it's for painting, or we can add a flag. Let's stick to "IsDown &&
  // Hover" for now, as it covers both cases (continuous paint vs single click
  // with external latch).

  DrawImage(x, y, w, h, texture, hover ? hoverColor : color);

  return hover && down;
}

void UISystem::EndFrame(VkCommandBuffer commandBuffer) {
#if VULKAN_SDK_AVAILABLE
  if (drawList.empty())
    return;

  // Bind Pipeline (UI Pipeline)
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    backend->graphicsPipeline);
  // WARNING: Using 'graphicsPipeline' which we forced to be UI pipeline in
  // VulkanBackend::CreateUIPipeline. In strict engine, we'd have a separate
  // 'uiPipeline' member.

  // Bind Vertex Buffers
  VkBuffer vertexBuffers[] = {quadMesh.vertexBuffer.buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(commandBuffer, quadMesh.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  GPUTexture *lastTex = nullptr;

  for (const auto &el : drawList) {
    if (!el.texture)
      continue;

    // Push Constants
    struct {
      glm::mat4 proj;
      glm::vec4 pos; // x, y, w, h
      glm::vec4 color;
    } pc;

    pc.proj = projection;
    pc.pos = glm::vec4(el.x, el.y, el.width, el.height);
    pc.color = el.color;

    vkCmdPushConstants(commandBuffer, backend->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);

    // Bind Texture if changed
    if (el.texture != lastTex) {
      backend->BindTexture(*el.texture, commandBuffer);
      lastTex = el.texture;
    }

    vkCmdDrawIndexed(commandBuffer, quadMesh.indexCount, 1, 0, 0, 0);
  }
#endif
}

} // namespace Graphics
} // namespace Mesozoic
