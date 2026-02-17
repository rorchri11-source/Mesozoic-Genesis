#include "../Assets/GLTFLoader.h"
#include "../Core/Simulation/SimulationManager.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/TerrainSystem.h"
#include "../Graphics/TerrainGenerator.h"
#include "../Assets/TextureLoader.h"
#include "../Graphics/UI/UISystem.h"
#include "../Graphics/Window.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <glm/glm.hpp> // Ensure GLM is included for vec3 math

using namespace Mesozoic;
using namespace Mesozoic::Graphics;
using namespace Mesozoic::Core;
using namespace Mesozoic::Assets;

// Game State Enum
enum class GameState { MENU, PLAYING };

// Editor State
bool editorMode = false;
int brushType = 0; // 0=Grass, 1=Dirt, 2=Rock
float brushRadius = 5.0f;

int main() {
  WindowConfig config;
  config.width = 1280;
  config.height = 720;
  config.title = "Mesozoic Eden - Pre-Alpha";

  Window window;
  if (!window.Initialize(config)) {
    return -1;
  }

  VulkanBackend backend;
  if (!backend.Initialize(window)) {
    return -1;
  }

  // Terrain System
  TerrainSystem terrainSystem;
  terrainSystem.Initialize(&backend);

  // UI System
  UISystem uiSystem;
  uiSystem.Initialize(&backend, &window);

  // Create UI Pipelines
  if (!backend.CreateUIPipeline("ui.vert.spv", "ui.frag.spv")) {
    std::cerr << "[Main] Failed to create UI Pipeline. UI might not render."
              << std::endl;
  }

  Renderer renderer;
  if (!renderer.Initialize(window, &backend)) {
    return -1;
  }

  // Connect Systems
  renderer.uiSystem = &uiSystem;
  renderer.terrainSystem = &terrainSystem;

  // Simulation
  SimulationManager sim;
  // sim.Initialize(); // Removed as it doesn't exist
  sim.terrainSystem = &terrainSystem;

  // --- ASSET LOADING ---

  // 1. White Texture for UI
  TextureLoader texLoader;
  std::vector<uint8_t> whitePx = {255, 255, 255, 255};
  GPUTexture whiteTex =
      backend.CreateTextureFromBuffer(whitePx.data(), whitePx.size(), 1, 1, VK_FORMAT_R8G8B8A8_UNORM);

  // 2. Initial Ecosystem
  std::cout << ">> Spawning initial ecosystem..." << std::endl;
  sim.SpawnDinosaur(Species::TRex);
  sim.SpawnDinosaur(Species::Triceratops);
  sim.SpawnDinosaur(Species::Brachiosaurus);

  // 3. Meshes
  auto gltfDino = Assets::GLTFLoader::CreateDinosaurPlaceholder(6.0f, 3.0f);
  UberMesh dinoMesh;
  for (const auto &vert : gltfDino.primitives[0].vertices) {
    Vertex v;
    v.position = {vert.position.x, vert.position.y, vert.position.z};
    v.normal = {vert.normal.x, vert.normal.y, vert.normal.z};
    v.uv = {vert.uv[0], vert.uv[1]};
    v.boneIndices = {0, 0, 0, 0};
    v.boneWeights = {1, 0, 0, 0};
    dinoMesh.baseVertices.push_back(v);
  }
  dinoMesh.indices = gltfDino.primitives[0].indices;
  uint32_t dinoMeshId = renderer.RegisterMesh(dinoMesh);

  UberMesh terrainMesh = TerrainGenerator::GenerateGrid(
      terrainSystem.width, terrainSystem.depth, terrainSystem.scale);
  uint32_t terrainMeshId = renderer.RegisterMesh(terrainMesh);

  UberMesh skyMesh;
  float skySize = 4000.0f;
  std::vector<Vec3> skyVerts = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1},
                                {-1, 1, -1},  {-1, -1, 1}, {1, -1, 1},
                                {1, 1, 1},    {-1, 1, 1}};
  for (auto v : skyVerts) {
    Vertex vert;
    vert.position = {v.x * skySize, v.y * skySize, v.z * skySize};
    vert.normal = {-v.x, -v.y, -v.z};
    skyMesh.baseVertices.push_back(vert);
  }
  skyMesh.indices = {0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7,
                     4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3};
  uint32_t skyMeshId = renderer.RegisterMesh(skyMesh);

  auto grassGltf = Assets::GLTFLoader::CreateGrassMesh(1.5f);
  UberMesh grassMesh;
  for (auto &v : grassGltf.primitives[0].vertices) {
    Vertex vert;
    vert.position = {v.position.x, v.position.y, v.position.z};
    vert.normal = {v.normal.x, v.normal.y, v.normal.z};
    grassMesh.baseVertices.push_back(vert);
  }
  grassMesh.indices = grassGltf.primitives[0].indices;
  uint32_t grassMeshId = renderer.RegisterMesh(grassMesh);

  // --- MAIN LOOP ---
  GameState currentState = GameState::MENU;
  window.SetCursorLocked(false);

  auto lastTime = std::chrono::high_resolution_clock::now();
  int frameCount = 0;
  float fpsTimer = 0.0f;

  while (!window.ShouldClose()) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    if (dt > 0.1f)
      dt = 0.1f;

    window.PollEvents();

    if (window.IsKeyPressed(27)) { // ESC
      if (currentState == GameState::PLAYING) {
        currentState = GameState::MENU;
        window.SetCursorLocked(false);
      }
    }

    uiSystem.BeginFrame();
    renderer.renderQueue.clear();

    if (currentState == GameState::MENU) {
      float screenW = uiSystem.GetScreenWidth();
      float screenH = uiSystem.GetScreenHeight();

      uiSystem.DrawImage(0, 0, screenW, screenH, whiteTex,
                         {0.1f, 0.1f, 0.15f, 0.9f});

      float btnW = 300, btnH = 60;
      float centerX = (screenW - btnW) / 2;
      float centerY = (screenH) / 2 - 100;

      if (uiSystem.DrawButton(centerX, centerY, btnW, btnH, whiteTex,
                              {0.2f, 0.7f, 0.3f, 1.0f},
                              {0.3f, 0.9f, 0.4f, 1.0f})) {
        currentState = GameState::PLAYING;
        window.SetCursorLocked(!editorMode);
      }

      if (uiSystem.DrawButton(centerX, centerY + 80, btnW, btnH, whiteTex,
                              {0.4f, 0.4f, 0.4f, 1.0f},
                              {0.6f, 0.6f, 0.6f, 1.0f})) {
        std::cout << "[Menu] Settings..." << std::endl;
      }

      if (uiSystem.DrawButton(centerX, centerY + 160, btnW, btnH, whiteTex,
                              {0.7f, 0.2f, 0.2f, 1.0f},
                              {0.9f, 0.3f, 0.3f, 1.0f})) {
        break;
      }

    } else if (currentState == GameState::PLAYING) {
      sim.Tick(dt);

      float moveSpeed = 10.0f * dt;
      if (window.IsKeyPressed(16))
        moveSpeed *= 3.0f;

      glm::vec3 fwd = renderer.camera.GetForward();
      glm::vec3 right = renderer.camera.GetRight();

      // Unified vec3 math
      if (window.IsKeyPressed('W')) renderer.camera.position += fwd * moveSpeed;
      if (window.IsKeyPressed('S')) renderer.camera.position -= fwd * moveSpeed;
      if (window.IsKeyPressed('D')) renderer.camera.position += right * moveSpeed;
      if (window.IsKeyPressed('A')) renderer.camera.position -= right * moveSpeed;

      if (!editorMode) {
        float dx, dy;
        window.GetMouseDelta(dx, dy);
        renderer.camera.Rotate(dx * 0.1f, dy * 0.1f);
      }

      float h = terrainSystem.GetHeight(renderer.camera.position.x,
                                        renderer.camera.position.z);
      if (renderer.camera.position.y < h + 2.0f)
        renderer.camera.position.y = h + 2.0f;

      // --- EDITOR LOGIC ---
      if (editorMode) {
        window.SetCursorLocked(false);

        float mx, my;
        window.GetMousePosition(mx, my);
        float ndcX = (mx / uiSystem.GetScreenWidth()) * 2.0f - 1.0f;
        float ndcY = (my / uiSystem.GetScreenHeight()) * 2.0f - 1.0f;

        float tanFov = tan(renderer.camera.fov * 0.5f * 3.14159f / 180.0f);
        float aspect = renderer.camera.aspectRatio;

        glm::vec3 cFwd = renderer.camera.GetForward();
        glm::vec3 cRight = renderer.camera.GetRight();
        glm::vec3 cUp = renderer.camera.GetUp();

        Vec3 vFwd(cFwd.x, cFwd.y, cFwd.z);
        Vec3 vRight(cRight.x, cRight.y, cRight.z);
        Vec3 vUp(cUp.x, cUp.y, cUp.z);

        Vec3 rayDir =
            (vFwd + vRight * (ndcX * aspect * tanFov) - vUp * (ndcY * tanFov))
                .Normalized();

        Vec3 hitPos;
        if (terrainSystem.Raycast(
                Vec3(renderer.camera.position.x, renderer.camera.position.y,
                     renderer.camera.position.z),
                rayDir, hitPos)) {
          if (window.IsMouseButtonDown(Window::MOUSE_LEFT)) {
            terrainSystem.Paint(hitPos.x, hitPos.z, brushRadius, brushType);
          }
        }
      }

      // --- RENDER SUBMISSION ---
      Matrix4 skyModel = Matrix4::Identity();
      skyModel.m[12] = renderer.camera.position.x;
      skyModel.m[13] = renderer.camera.position.y;
      skyModel.m[14] = renderer.camera.position.z;
      renderer.SubmitEntity({.entityId = 99998,
                             .meshIndex = skyMeshId,
                             .worldTransform = skyModel.m,
                             .color = {0, 0, 0, 0},
                             .visible = true});

      renderer.SubmitEntity(
          {.entityId = 99999,
           .meshIndex = terrainMeshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.2f, 0.4f, 0.1f, 1},
           .visible = true});

      for (const auto &dino : sim.entities) {
        if (!dino.vitals.alive)
          continue;
        RenderObject obj;
        obj.entityId = dino.id;
        Matrix4 m = Matrix4::Identity();
        float s = dino.transform.scale[0] * dino.genetics.sizeMultiplier;
        m.m[0] = s;
        m.m[5] = s;
        m.m[10] = s;
        m.m[12] = dino.transform.position[0];
        m.m[13] = dino.transform.position[1];
        m.m[14] = dino.transform.position[2];

        obj.worldTransform = m.m;
        obj.meshIndex = dinoMeshId;
        obj.color = (dino.species == Species::TRex)
                        ? std::array<float, 4>{0.8f, 0.3f, 0.2f, 1}
                        : std::array<float, 4>{0.2f, 0.7f, 0.3f, 1};
        renderer.SubmitEntity(obj);
      }

      renderer.SubmitEntity(
          {.entityId = 50000,
           .meshIndex = grassMeshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.1f, 0.8f, 0.2f, 0.5f},
           .visible = true});

      // --- IN-GAME UI ---
      float editorBtnW = 100;
      if (uiSystem.DrawButton(
              uiSystem.GetScreenWidth() - editorBtnW - 10, 10, editorBtnW, 40,
              whiteTex, {0.6f, 0.6f, 1.0f, 0.8f}, {0.8f, 0.8f, 1.0f, 1.0f})) {
        static float toggleCooldown = 0.0f;
        if (toggleCooldown <= 0.0f) {
          editorMode = !editorMode;
          window.SetCursorLocked(!editorMode);
          toggleCooldown = 0.5f;
        }
        toggleCooldown -= dt; // Not exact but enough for latch
      }
      static float cooldown = 0.0f;
      if (cooldown > 0) cooldown -= dt;

      if (editorMode) {
        uiSystem.DrawImage(10, 60, 200, 150, whiteTex, {0, 0, 0, 0.5f});

        if (uiSystem.DrawButton(20, 70, 180, 30, whiteTex,
                                brushType == 0 ? glm::vec4(0.8, 1, 0.8, 1) : glm::vec4(0.5, 0.5, 0.5, 1)))
             brushType = 0;

        if (uiSystem.DrawButton(20, 110, 180, 30, whiteTex,
                                brushType == 1 ? glm::vec4(1, 0.8, 0.8, 1) : glm::vec4(0.5, 0.5, 0.5, 1)))
             brushType = 1;

        if (uiSystem.DrawButton(20, 150, 180, 30, whiteTex,
                                brushType == 2 ? glm::vec4(0.8, 0.8, 1, 1) : glm::vec4(0.5, 0.5, 0.5, 1)))
             brushType = 2;
      }
    }

    renderer.camera.aspectRatio = (float)config.width / (float)config.height;

    Vec3 camPos(renderer.camera.position.x, renderer.camera.position.y,
                renderer.camera.position.z);
    Vec3 camFwd(renderer.camera.GetForward().x, renderer.camera.GetForward().y, renderer.camera.GetForward().z);

    renderer.camera.viewMatrix =
        Matrix4::LookAt(camPos, camPos + camFwd, Vec3(0, 1, 0)).m;

    Matrix4 proj = Matrix4::Perspective(
        renderer.camera.fov * 3.14159f / 180.0f, renderer.camera.aspectRatio,
        renderer.camera.nearPlane, renderer.camera.farPlane);
    renderer.camera.projMatrix = proj.m;

    renderer.RenderFrame(dt);

    frameCount++;
    fpsTimer += dt;
    if (fpsTimer >= 1.0f) {
      std::string title = config.title +
                          " | FPS: " + std::to_string(frameCount) +
                          " | Ents: " + std::to_string(sim.entities.size());
      window.SetTitle(title);
      frameCount = 0;
      fpsTimer = 0.0f;
    }
  }

  renderer.Cleanup();
  backend.Cleanup();
  window.Cleanup();
  return 0;
}
