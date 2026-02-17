#pragma once
#include "../VulkanBackend.h"
#include "../Window.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>


namespace Mesozoic {
namespace Graphics {

struct UIElement {
  float x, y, width, height;
  GPUTexture *texture;
  glm::vec4 color;
  bool isButton;
  bool visible;
  // Callback or ID could go here
};

class UISystem {
public:
  VulkanBackend *backend;
  Window *window;

  // Orthographic Matrix
  glm::mat4 projection;

  // Batch of draw calls
  std::vector<UIElement> drawList;

  // Quad mesh (shared)
  GPUMesh quadMesh;
  uint32_t quadMeshId;

  void Initialize(VulkanBackend *backend, Window *window);

  void BeginFrame();
  void EndFrame(VkCommandBuffer commandBuffer); // Record draw commands

  // Immediate Mode API
  void DrawImage(float x, float y, float w, float h, GPUTexture &texture,
                 glm::vec4 color = {1, 1, 1, 1});
  bool DrawButton(float x, float y, float w, float h, GPUTexture &texture,
                  glm::vec4 color = {1, 1, 1, 1},
                  glm::vec4 hoverColor = {0.8, 0.8, 0.8, 1});

  // Helper to get screen size
  float GetScreenWidth() const { return (float)window->config.width; }
  float GetScreenHeight() const { return (float)window->config.height; }

private:
  void CreateQuadMesh();
};

} // namespace Graphics
} // namespace Mesozoic
