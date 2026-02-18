#include "UISystem.h"
#include <cstring>
#include <iostream>

namespace Mesozoic {
namespace Graphics {

void UISystem::Initialize(VulkanBackend *backend, Window *window) {
  this->backend = backend;
  this->window = window;
  this->wasMouseLeftDown = false;
  this->isMouseJustPressed = false;
  CreateQuadMesh();
}

void UISystem::CreateQuadMesh() {
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

  // Upload to GPU using backend helper
  if (backend) {
      quadMesh = backend->UploadMesh(vertices, indices);
  }
}

void UISystem::BeginFrame() {
  drawList.clear();

  // Update Mouse State
  bool currentDown = window->IsMouseButtonDown(Window::MOUSE_LEFT);
  isMouseJustPressed = currentDown && !wasMouseLeftDown;
  wasMouseLeftDown = currentDown;

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

  glm::vec4 drawColor = color;
  if (hover) {
    if (down) {
      drawColor = hoverColor * 0.9f;
      drawColor.a = hoverColor.a; // Maintain alpha
    } else {
      drawColor = hoverColor;
    }
  }
  DrawImage(x, y, w, h, texture, drawColor);

  return hover && isMouseJustPressed;
}

bool UISystem::DrawSlider(float x, float y, float w, float h, float &value,
                          GPUTexture &texture, glm::vec4 color,
                          glm::vec4 knobColor) {
  float mx, my;
  window->GetMousePosition(mx, my);

  // Background
  DrawImage(x, y, w, h, texture, color);

  bool hover = (mx >= x && mx <= x + w && my >= y && my <= y + h);
  bool down = window->IsMouseButtonDown(Window::MOUSE_LEFT);
  bool changed = false;

  if (hover && down) {
    float newVal = (mx - x) / w;
    if (newVal < 0)
      newVal = 0;
    if (newVal > 1)
      newVal = 1;
    if (std::abs(newVal - value) > 0.001f) {
      value = newVal;
      changed = true;
    }
  }

  // Knob
  float knobW = 10.0f;
  float kx = x + value * (w - knobW);
  DrawImage(kx, y, knobW, h, texture, knobColor);

  return changed;
}

void UISystem::EndFrame(VkCommandBuffer commandBuffer) {
#if VULKAN_SDK_AVAILABLE
  if (drawList.empty())
    return;

  // Bind Pipeline (UI Pipeline)
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    backend->uiPipeline);

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
