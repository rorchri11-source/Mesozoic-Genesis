#pragma once
#include "../Core/Math/Matrix4.h"
#include "GPUAnimationInstancing.h"
#include "MorphingSystem.h"
#include "PBRSkinShader.h"
#include "UI/UISystem.h"
#include "UberMesh.h"
#include "VulkanBackend.h"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Graphics {

class Window;
class TerrainSystem;
// class UISystem; // Included above

// =========================================================================
// Render Object: what the renderer sees for each entity
// =========================================================================
struct RenderObject {
  uint32_t entityId;
  uint32_t meshIndex;                   // Index into MeshRegistry
  uint32_t materialIndex;               // Index into MaterialRegistry
  uint32_t animInstanceIndex;           // Index into GPUAnimationSystem
  std::array<float, 16> worldTransform; // 4x4 model matrix
  std::vector<float> morphWeights;      // DNA-driven morph weights
  std::array<float, 4> color = {1, 1, 1, 1};
  bool visible = true;
  float distanceToCamera = 0.0f; // For LOD and sorting
};

// =========================================================================
// Camera
// =========================================================================
struct Camera {
  glm::vec3 position = {0, 50, -100};
  glm::vec3 target = {0, 0, 0};
  glm::vec3 up = {0, 1, 0};
  float fov = 60.0f; // degrees
  float nearPlane = 0.1f;
  float farPlane = 5000.0f;
  float aspectRatio = 16.0f / 9.0f;

  // View-Projection matrix (would be computed with GLM or custom math)
  std::array<float, 16> viewMatrix;
  std::array<float, 16> projMatrix;

  // Helper methods
  glm::vec3 GetForward() const { return glm::normalize(target - position); }

  glm::vec3 GetRight() const {
    return glm::normalize(glm::cross(GetForward(), up));
  }

  glm::vec3 GetUp() const { return glm::cross(GetRight(), GetForward()); }

  void Rotate(float yaw, float pitch) {
    glm::vec3 fwd = GetForward();

    // Rotate around world up (0,1,0) for yaw
    glm::mat4 yawRot = glm::rotate(glm::mat4(1.0f), -yaw, glm::vec3(0, 1, 0));
    fwd = glm::vec3(yawRot * glm::vec4(fwd, 0.0f));

    // Rotate around right vector for pitch
    glm::vec3 right = glm::cross(fwd, glm::vec3(0, 1, 0));
    glm::mat4 pitchRot = glm::rotate(glm::mat4(1.0f), -pitch, right);
    fwd = glm::vec3(pitchRot * glm::vec4(fwd, 0.0f));

    // Update target
    target = position + fwd;
  }
};

// =========================================================================
// Scene-wide uniform data (uploaded to GPU each frame)
// =========================================================================
struct SceneUniforms {
  std::array<float, 16> viewProjection;
  std::array<float, 3> cameraPosition;
  float time;
  std::array<float, 3> sunDirection;
  float sunIntensity;
  std::array<float, 3> sunColor;
  float ambientIntensity;
  std::array<float, 3> windDirection; // For smell grid advection + foliage
  float windStrength;
};

// =========================================================================
// LOD (Level of Detail) system
// =========================================================================
struct LODLevel {
  float maxDistance;    // Switch to this LOD below this distance
  uint32_t meshIndex;   // Different mesh resolution
  bool useMorphTargets; // Disable morphing at far LODs
};

struct LODConfig {
  std::vector<LODLevel> levels; // Sorted near to far

  uint32_t SelectLOD(float distance) const {
    for (size_t i = 0; i < levels.size(); ++i) {
      if (distance < levels[i].maxDistance) {
        return static_cast<uint32_t>(i);
      }
    }
    return static_cast<uint32_t>(levels.size() - 1); // Furthest LOD
  }
};

// =========================================================================
// Main Renderer: orchestrates the entire frame
// =========================================================================
class Renderer {
public:
  VulkanBackend *backend = nullptr;
  Window *window = nullptr;
  Camera camera;
  SceneUniforms sceneData;

  // Registries
  std::vector<UberMesh> meshRegistry;
  std::vector<GPUMesh> gpuMeshes;
  SkinShaderSystem skinSystem;
  GPUAnimationSystem animSystem;

  // External Systems
  TerrainSystem *terrainSystem = nullptr;
  UISystem *uiSystem = nullptr;

  // Render queue
  std::vector<RenderObject> renderQueue;

  // LOD configuration per species
  std::vector<LODConfig> lodConfigs;

  // Day/Night Cycle
  float dayTime = 8.0f;      // 0..24 hours
  float daySpeed = 0.1f;     // Hours per second (realtime = 1/3600 ~ 0.00027)
  bool isDayCyclePaused = false;

  // Stats
  uint32_t drawCallsThisFrame = 0;
  uint32_t trianglesThisFrame = 0;
  uint32_t instancesThisFrame = 0;

  bool Initialize(Window &win, VulkanBackend *backendPtr) {
    window = &win;
    backend = backendPtr;

    if (!backend || !backend->initialized) {
      std::cerr << "[Renderer] Backend not initialized!" << std::endl;
      return false;
    }

    // Setup default LOD config for dinosaurs
    LODConfig defaultLOD;
    defaultLOD.levels = {
        {50.0f, 0, true},   // LOD0: Full detail + morphing
        {150.0f, 1, true},  // LOD1: Medium detail + morphing
        {500.0f, 2, false}, // LOD2: Low detail, no morphing
        {2000.0f, 3, false} // LOD3: Billboard/impostor
    };
    lodConfigs.push_back(defaultLOD);

    std::cout << "[Renderer] Initialized with " << defaultLOD.levels.size()
              << " LOD levels" << std::endl;
    return true;
  }

  uint32_t RegisterMesh(const UberMesh &mesh) {
    if (!backend || !backend->initialized)
      return 0xFFFFFFFF;
    GPUMesh gpuMesh = backend->UploadMesh(mesh.baseVertices, mesh.indices);
    gpuMeshes.push_back(gpuMesh);
    meshRegistry.push_back(mesh); // Keep CPU copy for metadata
    return (uint32_t)gpuMeshes.size() - 1;
  }

  void UpdateMesh(uint32_t meshId, const std::vector<Vertex> &vertices) {
    if (meshId >= gpuMeshes.size())
      return;

    size_t dataSize = vertices.size() * sizeof(Vertex);
    if (gpuMeshes[meshId].vertexBuffer.size < dataSize) {
      // If buffer is too small, destroy and recreate
      backend->DestroyBuffer(gpuMeshes[meshId].vertexBuffer);
      // Re-create vertex buffer
      // Note: Indices remain same, indexCount remains same
      // This is a simplified "Update" for vertex data only.
      // Re-upload logic:
      // We can't reuse UploadMesh because it returns a full GPUMesh struct.
      // We just need a new GPUBuffer for vertices.

      // For now, let's assume size is constant (terrain grid doesn't change resolution).
      std::cerr << "[Renderer] Warning: UpdateMesh called with larger size than buffer. Ignoring resize." << std::endl;
      return;
    }

    backend->UpdateBuffer(gpuMeshes[meshId].vertexBuffer, vertices.data(), dataSize);
  }

  void RenderFrame(float deltaTime) {
    drawCallsThisFrame = 0;
    trianglesThisFrame = 0;
    instancesThisFrame = 0;

    // Update scene uniforms
    if (!isDayCyclePaused) {
        dayTime += daySpeed * deltaTime;
        if (dayTime >= 24.0f) dayTime -= 24.0f;
    }

    // Map dayTime (0..24) to shader time (0..2PI approx or just linear)
    // Shader expects float time.
    // We can pass dayTime directly, but shader needs to wrap it or use sin/cos.
    // Let's pass dayTime as "time".
    sceneData.time = dayTime;

    // 1. Acquire swapchain image
    uint32_t imageIndex = 0;
    if (!backend->BeginFrame(imageIndex))
      return;

    // Single pass for now (since we currently have one simplified RenderPass)
    backend->BeginRenderPass(RenderPassType::GBuffer, imageIndex);

    RenderShadows();
    RenderGBuffer();
    RenderLighting();
    RenderSSS();
    RenderPostProcess();
    RenderUI(imageIndex);

    backend->EndRenderPass();

    // 8. Present
    backend->EndFrame(imageIndex);
  }

  void SubmitEntity(const RenderObject &obj) { renderQueue.push_back(obj); }

  void PrintStats() const {
    std::cout << "[Frame Stats] Draw Calls: " << drawCallsThisFrame
              << " | Triangles: " << trianglesThisFrame
              << " | Instances: " << instancesThisFrame << std::endl;
  }

  void Cleanup() {
    if (backend) {
      backend->WaitIdle();
      for (auto &mesh : gpuMeshes) {
        backend->DestroyMesh(mesh);
      }
    }
    gpuMeshes.clear();
    // Do not clean up backend here as we do not own it
  }

private:
  void RenderShadows() { drawCallsThisFrame++; }

  void RenderGBuffer() {
    backend->BindPipeline(backend->graphicsPipeline);
    backend->BindTerrainTextures();

    for (const auto &obj : renderQueue) {
      if (!obj.visible)
        continue;

      if (obj.meshIndex >= gpuMeshes.size())
        continue;

      // Compute MVP
      Mesozoic::Math::Matrix4 view, proj;
      view.m = camera.viewMatrix;
      proj.m = camera.projMatrix;
      Mesozoic::Math::Matrix4 vp = proj * view;

      Mesozoic::Math::Matrix4 model;
      model.m = obj.worldTransform;
      Mesozoic::Math::Matrix4 mvp = vp * model;

      // Push Constants (Model Matrix + Color + Time + CamPos + ModelPos +
      // Morphs)
      struct PushData {
        Mesozoic::Math::Matrix4 mvp;
        std::array<float, 4> color;
        float time;
        float camX, camY, camZ;
        float modX, modY, modZ;
        float padding;
        std::array<float, 4> morphWeights;
        uint32_t vertexCount;
      } push;
      push.mvp = mvp;
      push.color = obj.color;
      push.time = sceneData.time;
      push.camX = camera.position.x;
      push.camY = camera.position.y;
      push.camZ = camera.position.z;
      push.modX = obj.worldTransform[12];
      push.modY = obj.worldTransform[13];
      push.modZ = obj.worldTransform[14];

      // Default to 0
      push.morphWeights = {0, 0, 0, 0};
      push.vertexCount = 0;

      // If object has morph weights, use them
      if (!obj.morphWeights.empty()) {
        for (size_t i = 0; i < obj.morphWeights.size() && i < 4; ++i) {
          push.morphWeights[i] = obj.morphWeights[i];
        }
        // Assuming meshIndex corresponds to registered mesh, we need vertex
        // count gpuMeshes doesn't store vertex count in easily accessible way?
        // GPUMesh struct has indexCount. But we need VERTEX count for shader
        // indexing? Wait, shader uses gl_VertexIndex. If we use separate buffer
        // for deltas, we need to know the offset or assume 1:1 mapping. If we
        // bind the buffer for THIS mesh, then offset is 0. But we bind ONE
        // global buffer? No, plan is to use ONE buffer for Dino and bind it
        // globally. Terrain renders with vertexCount=0, so shader skips
        // morphing. Dino renders with vertexCount=DinoVerts. We need to know
        // DinoVerts. gpuMeshes doesn't store it. meshRegistry stores UberMesh
        // which has baseVertices.size().
        if (obj.meshIndex < meshRegistry.size()) {
          push.vertexCount =
              (uint32_t)meshRegistry[obj.meshIndex].baseVertices.size();
        }
      }
      backend->PushConstants(backend->pipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT,
                             0, sizeof(PushData), &push);

      // Special check for grass instancing (flagged by alpha 0.5)
      if (abs(obj.color[3] - 0.5f) < 0.01f) {
        backend->DrawMeshInstanced(gpuMeshes[obj.meshIndex], 800000);
        instancesThisFrame += 800000;
      } else {
        backend->DrawMesh(gpuMeshes[obj.meshIndex]);
        instancesThisFrame++;
      }

      drawCallsThisFrame++;
    }
  }

  void RenderLighting() { drawCallsThisFrame++; }

  void RenderSSS() { drawCallsThisFrame++; }

  void RenderPostProcess() {
    drawCallsThisFrame += 4; // Multiple passes
  }

  void RenderUI(uint32_t imageIndex) {
    if (uiSystem) {
      VkCommandBuffer cmd = backend->GetCommandBuffer(imageIndex);
      if (cmd) {
        uiSystem->EndFrame(cmd);
      }
    }
    drawCallsThisFrame++;
  }
};

} // namespace Graphics
} // namespace Mesozoic
