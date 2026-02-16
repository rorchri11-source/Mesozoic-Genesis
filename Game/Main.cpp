#include "../AssetLoaders/GLTFLoader.h" // Fixed include path based on file structure
#include "../Core/Simulation/SimulationManager.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/TerrainSystem.h"
#include "../Graphics/TextureLoader.h" // For loading white texture
#include "../Graphics/UI/UISystem.h"
#include "../Graphics/Window.h"
#include <chrono>
#include <iostream>
#include <thread> // For sleep


using namespace Mesozoic;
using namespace Mesozoic::Graphics;
using namespace Mesozoic::Core;

// Game State Enum
enum class GameState { MENU, PLAYING };

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
  // Note: We need shaders for UI. Assuming we compiled them.
  // ui.vert.spv and ui.frag.spv
  if (!backend.CreateUIPipeline("Shaders/ui.vert.spv", "Shaders/ui.frag.spv")) {
    std::cerr << "[Main] Failed to create UI Pipeline. UI might not render."
              << std::endl;
  }

  Renderer renderer;
  if (!renderer.Initialize(window)) { // Renderer::Initialize takes Window&
    return -1;
  }

  // Connect UI System to Renderer
  renderer.uiSystem = &uiSystem;

  // Simulation
  SimulationManager sim;
  sim.Initialize();
  sim.terrainSystem = &terrainSystem; // Connect Terrain

  // --- ASSET LOADING ---

  // 1. White Texture for UI
  TextureLoader texLoader;
  std::vector<uint8_t> whitePx = {255, 255, 255, 255};
  GPUTexture whiteTex =
      backend.CreateTexture(whitePx.data(), 1, 1, VK_FORMAT_R8G8B8A8_UNORM);

  // 2. Initial Ecosystem
  std::cout << ">> Spawning initial ecosystem..." << std::endl;
  sim.SpawnDinosaur(Species::TRex);
  sim.SpawnDinosaur(Species::Triceratops);
  sim.SpawnDinosaur(Species::Brachiosaurus);

  // 3. Meshes (Dino, Sky, Terrain) - Copying logic from previous Main.cpp
  // We need to re-register these because we overwrote Main.cpp

  // Dino Mesh
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

  // Terrain Mesh (Sync with Texture System)
  UberMesh terrainMesh = TerrainGenerator::GenerateGrid(512, 512, 3.0f);
  for (auto &v : terrainMesh.baseVertices) {
    float h = terrainSystem.GetHeight(v.position[0], v.position[2]);
    v.position[1] = h;
    Vec3 n = terrainSystem.GetNormal(v.position[0], v.position[2]);
    v.normal = {n.x, n.y, n.z};
  }
  uint32_t terrainMeshId = renderer.RegisterMesh(terrainMesh);

  // Skybox
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

  // Grass Mesh
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

    // Global Inputs
    if (window.IsKeyPressed(27)) { // ESC
      if (currentState == GameState::PLAYING) {
        currentState = GameState::MENU;
        window.SetCursorLocked(false);
      } else {
        // If already in menu, close? Or simple toggle?
        // Let's just break if in Menu
        break;
      }
    }

    // Prepare Frame Logic
    uiSystem.BeginFrame();
    renderer.renderQueue.clear();

    // --- STATE MACHINE ---
    if (currentState == GameState::MENU) {
      // Draw Main Menu
      float screenW = (float)config.width;
      float screenH =
          (float)config.height; // Wait, WindowConfig vs actual size?
      // Use uiSystem helpers
      screenW = uiSystem.GetScreenWidth();
      screenH = uiSystem.GetScreenHeight();

      // Background (Darkened Overlay)
      uiSystem.DrawImage(0, 0, screenW, screenH, whiteTex,
                         {0.1f, 0.1f, 0.15f, 1.0f});

      // Buttons
      float btnW = 300, btnH = 60;
      float centerX = (screenW - btnW) / 2;
      float centerY = (screenH) / 2 - 100;

      // START
      if (uiSystem.DrawButton(centerX, centerY, btnW, btnH, whiteTex,
                              {0.2f, 0.7f, 0.3f, 1.0f},
                              {0.3f, 0.9f, 0.4f, 1.0f})) {
        currentState = GameState::PLAYING;
        window.SetCursorLocked(true);
      }

      // SETTINGS
      if (uiSystem.DrawButton(centerX, centerY + 80, btnW, btnH, whiteTex,
                              {0.4f, 0.4f, 0.4f, 1.0f},
                              {0.6f, 0.6f, 0.6f, 1.0f})) {
        std::cout << "[Menu] Settings..." << std::endl;
      }

      // EXIT
      if (uiSystem.DrawButton(centerX, centerY + 160, btnW, btnH, whiteTex,
                              {0.7f, 0.2f, 0.2f, 1.0f},
                              {0.9f, 0.3f, 0.3f, 1.0f})) {
        break;
      }

    } else if (currentState == GameState::PLAYING) {
      // --- GAMEPLAY LOGIC ---

      // 1. Simulation Tick
      sim.Tick(dt);

      // 2. Camera Input
      float moveSpeed = 10.0f * dt;
      if (window.IsKeyPressed(16))
        moveSpeed *= 3.0f; // Shift

      glm::vec3 fwd = renderer.camera.GetForward();
      glm::vec3 right = renderer.camera.GetRight();

      if (window.IsKeyPressed('W'))
        renderer.camera.position += fwd * moveSpeed;
      if (window.IsKeyPressed('S'))
        renderer.camera.position -= fwd * moveSpeed;
      if (window.IsKeyPressed('D'))
        renderer.camera.position += right * moveSpeed;
      if (window.IsKeyPressed('A'))
        renderer.camera.position -= right * moveSpeed;

      float dx, dy;
      window.GetMouseDelta(dx, dy);
      renderer.camera.Rotate(dx * 0.1f, dy * 0.1f);

      // Terrain Constraint
      float h = terrainSystem.GetHeight(renderer.camera.position.x,
                                        renderer.camera.position.z);
      if (renderer.camera.position.y < h + 2.0f)
        renderer.camera.position.y = h + 2.0f;

      // 3. Render Submission (World)

      // Skybox
      Matrix4 skyModel = Matrix4::Identity();
      skyModel.m[12] = renderer.camera.position[0];
      skyModel.m[13] = renderer.camera.position[1];
      skyModel.m[14] = renderer.camera.position[2];
      renderer.SubmitEntity({.entityId = 99998,
                             .meshIndex = skyMeshId,
                             .worldTransform = skyModel.m,
                             .color = {0, 0, 0, 0},
                             .visible = true});

      // Terrain
      renderer.SubmitEntity(
          {.entityId = 99999,
           .meshIndex = terrainMeshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.2f, 0.4f, 0.1f, 1},
           .visible = true});

      // Dinosaurs
      for (const auto &dino : sim.entities) {
        if (!dino.vitals.alive)
          continue;
        RenderObject obj;
        obj.entityId = dino.id;
        // Simplified transform construction
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

      // Grass (Use Alpha 0.5 flag)
      renderer.SubmitEntity(
          {.entityId = 50000,
           .meshIndex = grassMeshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.1f, 0.8f, 0.2f, 0.5f},
           .visible = true});

      // 4. In-Game HUD
      uiSystem.DrawImage(10, 10, 200, 60, whiteTex,
                         {0, 0, 0, 0.5f}); // Stats box

      // Toggle Editor Button (Top Right)
      float editorBtnW = 60;
      if (uiSystem.DrawButton(
              uiSystem.GetScreenWidth() - editorBtnW - 10, 10, editorBtnW, 60,
              whiteTex, {0.6f, 0.6f, 1.0f, 1.0f}, {0.8f, 0.8f, 1.0f, 1.0f})) {
        std::cout << "[Game] Editor Toggle Clicked" << std::endl;
      }
    }

    // Common: Setup Camera Matrices
    renderer.camera.aspectRatio = (float)config.width / (float)config.height;
    // View/Proj calc inside Main is duplicated in RenderGBuffer logic?
    // Renderer.h has Camera struct but RenderGBuffer re-computes from it.
    // We just need to ensure renderer.camera position/matrices are set.
    // LookAt calculation:
    Vec3 camPos(renderer.camera.position[0], renderer.camera.position[1],
                renderer.camera.position[2]);
    Vec3 camFwd = renderer.camera.GetForward();
    renderer.camera.viewMatrix =
        Matrix4::LookAt(camPos, camPos + camFwd, Vec3(0, 1, 0)).m;

    Matrix4 proj = Matrix4::Perspective(
        renderer.camera.fov * 3.14159f / 180.0f, renderer.camera.aspectRatio,
        renderer.camera.nearPlane, renderer.camera.farPlane);
    renderer.camera.projMatrix = proj.m;

    // Render Frame
    renderer.RenderFrame(dt);

    // FPS stats
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

  // Cleanup
  renderer.Cleanup();
  window.Cleanup();
  return 0;
}
