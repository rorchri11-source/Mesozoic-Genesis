// Resolved Conflicts
#include "../AssetLoaders/GLTFLoader.h"
#include "../Core/Simulation/SimulationManager.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/TerrainSystem.h"
#include "../Graphics/TextureLoader.h"
#include "../Graphics/UI/UISystem.h"
#include "../Graphics/Window.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <iomanip>
#include <sstream>

using namespace Mesozoic;
using namespace Mesozoic::Graphics;
using namespace Mesozoic::Core;

// Game State Enum
enum class GameState { MENU, PLAYING, EDITOR };

// Brush Modes
enum class BrushMode { NONE, RAISE, LOWER, FLATTEN };

int main() {
  WindowConfig config;
  config.width = 1280;
  config.height = 720;
  config.title = "Mesozoic Eden - Pre-Alpha";

  Window window;
  if (!window.Initialize(config)) {
    return -1;
  }

  // Initialize Renderer first (creates VulkanBackend)
  Renderer renderer;
  if (!renderer.Initialize(window)) {
    return -1;
  }

  VulkanBackend& backend = renderer.backend;

  // Initialize Terrain System (needs Renderer)
  TerrainSystem terrainSystem;
  terrainSystem.Initialize(&renderer, 512, 512, 3.0f, 50.0f);
  renderer.terrainSystem = &terrainSystem;

  // Initialize UI System (needs Backend & Window)
  UISystem uiSystem;
  uiSystem.Initialize(&backend, &window);
  renderer.uiSystem = &uiSystem;

  // Create UI Pipelines
  if (!backend.CreateUIPipeline("Shaders/ui.vert.spv", "Shaders/ui.frag.spv")) {
    std::cerr << "[Main] Failed to create UI Pipeline. UI might not render."
              << std::endl;
  }

  // Simulation
  SimulationManager sim;
  sim.Initialize();
  sim.terrainSystem = &terrainSystem;

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

  // 3. Meshes (Dino, Sky, Grass)
  // Terrain mesh is now handled by TerrainSystem::Initialize

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

  // --- EDITOR STATE ---
  BrushMode brushMode = BrushMode::NONE;
  float brushSize = 10.0f;
  float brushStrength = 5.0f;
  float flattenTargetHeight = 0.0f;
  bool isFlattenTargetSet = false;

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
      if (currentState == GameState::PLAYING || currentState == GameState::EDITOR) {
        currentState = GameState::MENU;
        window.SetCursorLocked(false);
      } else {
        break;
      }
    }

    // Prepare Frame Logic
    uiSystem.BeginFrame();
    renderer.renderQueue.clear();

    float screenW = uiSystem.GetScreenWidth();
    float screenH = uiSystem.GetScreenHeight();

    // --- STATE MACHINE ---
    if (currentState == GameState::MENU) {
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

      // EDITOR
      if (uiSystem.DrawButton(centerX, centerY + 80, btnW, btnH, whiteTex,
                              {0.2f, 0.5f, 0.8f, 1.0f},
                              {0.4f, 0.7f, 0.9f, 1.0f})) {
        currentState = GameState::EDITOR;
        window.SetCursorLocked(false); // Editor needs cursor
      }

      // EXIT
      if (uiSystem.DrawButton(centerX, centerY + 160, btnW, btnH, whiteTex,
                              {0.7f, 0.2f, 0.2f, 1.0f},
                              {0.9f, 0.3f, 0.3f, 1.0f})) {
        break;
      }

    } else if (currentState == GameState::PLAYING || currentState == GameState::EDITOR) {
      // --- GAMEPLAY/EDITOR LOGIC ---

      // 1. Simulation Tick
      if (!renderer.isDayCyclePaused) {
          sim.Tick(dt);
      }

      // 2. Camera Input
      // In Editor mode, only move camera if Right Mouse is held?
      // Or standard WASD + Mouse Look always?
      // Let's allow movement always, but for Editor we need cursor for UI.
      // If Editor -> Cursor Unlocked. Camera rotation requires Right Click Drag?
      // If Playing -> Cursor Locked. Always rotate.

      bool canMoveCamera = (currentState == GameState::PLAYING) ||
                           (currentState == GameState::EDITOR && window.IsMouseButtonDown(1)); // Right Click

      if (canMoveCamera) {
          float moveSpeed = 20.0f * dt;
          if (window.IsKeyPressed(16)) moveSpeed *= 3.0f; // Shift

          glm::vec3 fwd = renderer.camera.GetForward();
          glm::vec3 right = renderer.camera.GetRight();

          if (window.IsKeyPressed('W')) renderer.camera.position += fwd * moveSpeed;
          if (window.IsKeyPressed('S')) renderer.camera.position -= fwd * moveSpeed;
          if (window.IsKeyPressed('D')) renderer.camera.position += right * moveSpeed;
          if (window.IsKeyPressed('A')) renderer.camera.position -= right * moveSpeed;
          if (window.IsKeyPressed('E')) renderer.camera.position[1] += moveSpeed;
          if (window.IsKeyPressed('Q')) renderer.camera.position[1] -= moveSpeed;

          float dx, dy;
          window.GetMouseDelta(dx, dy);
          renderer.camera.Rotate(dx * 0.1f, dy * 0.1f);
      }

      // Terrain Constraint (only in Playing mode, or always?)
      // In Editor, we want to fly.
      if (currentState == GameState::PLAYING) {
        float h = terrainSystem.GetHeight(renderer.camera.position.x,
                                            renderer.camera.position.z);
        if (renderer.camera.position.y < h + 2.0f)
            renderer.camera.position.y = h + 2.0f;
      }

      // 3. Editor Logic
      if (currentState == GameState::EDITOR) {
          float mx, my;
          window.GetMousePosition(mx, my);

          // UI Panel Area (Right side, 300px)
          float panelW = 300.0f;
          float panelX = screenW - panelW;
          bool mouseOverUI = (mx > panelX);

          // Brush Logic
          if (!mouseOverUI && brushMode != BrushMode::NONE && window.IsMouseButtonDown(0)) {
              // Raycast
              // Convert Mouse (Screen) to World Ray
              // Need Inverse ViewProj?
              // Simple approximation: Camera Forward? No, mouse picking required.
              // For now, since we don't have Unproject implemented easily,
              // let's just raycast from camera center if cursor is locked?
              // But cursor is unlocked.
              // We need ScreenToWorldRay.
              // Given time constraints, let's use Camera Position + Forward * distance?
              // No, that's center of screen.
              // Let's implement basic Raycast from center of screen for now
              // (assuming user points camera at terrain).
              // Or if we want mouse cursor picking, we need unprojection.

              // Let's use Camera Forward for painting (Paint where looking).
              // It's intuitive enough for "FPS style" editor.
              Vec3 origin(renderer.camera.position[0], renderer.camera.position[1], renderer.camera.position[2]);
              glm::vec3 fwdGLM = renderer.camera.GetForward();
              Vec3 dir(fwdGLM.x, fwdGLM.y, fwdGLM.z);

              float hitT;
              Vec3 hitPoint;
              if (terrainSystem.Raycast(origin, dir, hitT, hitPoint)) {
                  // Flatten Target Logic
                  if (brushMode == BrushMode::FLATTEN) {
                      if (!isFlattenTargetSet) {
                          flattenTargetHeight = hitPoint.y;
                          isFlattenTargetSet = true;
                      }
                  } else {
                      isFlattenTargetSet = false;
                  }

                  int modeInt = 0;
                  if (brushMode == BrushMode::RAISE) modeInt = 0;
                  if (brushMode == BrushMode::LOWER) modeInt = 1;
                  if (brushMode == BrushMode::FLATTEN) modeInt = 2;

                  terrainSystem.ModifyHeight(hitPoint.x, hitPoint.z, brushSize, brushStrength * dt, modeInt, flattenTargetHeight);
              }
          } else {
              if (!window.IsMouseButtonDown(0)) {
                  isFlattenTargetSet = false; // Reset flatten target on mouse release
              }
          }
      }

      // 4. Render Submission

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

      // Terrain (Mesh ID stored in TerrainSystem)
      renderer.SubmitEntity(
          {.entityId = 99999,
           .meshIndex = terrainSystem.meshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.2f, 0.4f, 0.1f, 1},
           .visible = true});

      // Dinosaurs
      for (const auto &dino : sim.entities) {
        if (!dino.vitals.alive)
          continue;
        RenderObject obj;
        obj.entityId = dino.id;
        Matrix4 m = Matrix4::Identity();
        float s = dino.transform.scale[0] * dino.genetics.sizeMultiplier;
        m.m[0] = s; m.m[5] = s; m.m[10] = s;
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

      // Grass
      renderer.SubmitEntity(
          {.entityId = 50000,
           .meshIndex = grassMeshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.1f, 0.8f, 0.2f, 0.5f},
           .visible = true});

      // 5. UI Overlay

      // Top Info
      std::stringstream ss;
      ss << "Time: " << std::fixed << std::setprecision(1) << renderer.dayTime << "h";
      // TODO: DrawText not available in UISystem, assume debug output for now or add font later.

      if (currentState == GameState::EDITOR) {
          // Editor Panel Background
          float panelW = 300;
          uiSystem.DrawImage(screenW - panelW, 0, panelW, screenH, whiteTex, {0.2f, 0.2f, 0.2f, 0.8f});

          float x = screenW - panelW + 20;
          float y = 50;

          // Day/Night Controls
          // Speed
          if (uiSystem.DrawButton(x, y, 40, 30, whiteTex, {0.4f, 0.4f, 0.4f, 1}, {0.5f,0.5f,0.5f,1})) {
              renderer.daySpeed = std::max(0.0f, renderer.daySpeed - 0.1f);
          }
          if (uiSystem.DrawButton(x + 50, y, 40, 30, whiteTex, {0.4f, 0.4f, 0.4f, 1}, {0.5f,0.5f,0.5f,1})) {
              renderer.daySpeed += 0.1f;
          }
          // Pause
          if (uiSystem.DrawButton(x + 100, y, 80, 30, whiteTex,
                renderer.isDayCyclePaused ? glm::vec4(0.8,0.2,0.2,1) : glm::vec4(0.2,0.8,0.2,1),
                {0.5f,0.5f,0.5f,1})) {
              renderer.isDayCyclePaused = !renderer.isDayCyclePaused;
          }
          y += 50;

          // Time Slider (Simple Click to set)
          float sliderW = 200;
          uiSystem.DrawImage(x, y, sliderW, 10, whiteTex, {0,0,0,1});
          float knobX = x + (renderer.dayTime / 24.0f) * sliderW;
          uiSystem.DrawImage(knobX - 5, y - 5, 10, 20, whiteTex, {1,1,0,1});

          // Interaction for Slider
          float mx, my;
          window.GetMousePosition(mx, my);
          if (window.IsMouseButtonDown(0) && mx >= x && mx <= x + sliderW && my >= y - 10 && my <= y + 20) {
              float t = (mx - x) / sliderW;
              renderer.dayTime = t * 24.0f;
          }
          y += 50;

          // Brush Modes
          auto drawModeBtn = [&](const std::string& name, BrushMode mode, float offsetY) {
              bool active = (brushMode == mode);
              glm::vec4 col = active ? glm::vec4(0.3, 0.8, 0.3, 1) : glm::vec4(0.4, 0.4, 0.4, 1);
              if (uiSystem.DrawButton(x, y + offsetY, 200, 30, whiteTex, col, {0.5,0.5,0.5,1})) {
                  brushMode = mode;
              }
          };

          drawModeBtn("Raise", BrushMode::RAISE, 0);
          drawModeBtn("Lower", BrushMode::LOWER, 40);
          drawModeBtn("Flatten", BrushMode::FLATTEN, 80);
          drawModeBtn("None", BrushMode::NONE, 120);
          y += 180;

          // Brush Size
          uiSystem.DrawImage(x, y, sliderW, 10, whiteTex, {0,0,0,1});
          float sizeKnobX = x + (brushSize / 50.0f) * sliderW;
          uiSystem.DrawImage(sizeKnobX - 5, y - 5, 10, 20, whiteTex, {0.8,0.8,1,1});
          if (window.IsMouseButtonDown(0) && mx >= x && mx <= x + sliderW && my >= y - 10 && my <= y + 20) {
              float t = (mx - x) / sliderW;
              brushSize = t * 50.0f;
              if(brushSize < 1.0f) brushSize = 1.0f;
          }
          y += 40;

          // Brush Strength
          uiSystem.DrawImage(x, y, sliderW, 10, whiteTex, {0,0,0,1});
          float strKnobX = x + (brushStrength / 20.0f) * sliderW;
          uiSystem.DrawImage(strKnobX - 5, y - 5, 10, 20, whiteTex, {1,0.5,0.5,1});
           if (window.IsMouseButtonDown(0) && mx >= x && mx <= x + sliderW && my >= y - 10 && my <= y + 20) {
              float t = (mx - x) / sliderW;
              brushStrength = t * 20.0f;
          }
      }
    }

    // Common: Setup Camera Matrices
    renderer.camera.aspectRatio = (float)config.width / (float)config.height;

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
                          " | Ents: " + std::to_string(sim.entities.size()) +
                          " | Time: " + std::to_string((int)renderer.dayTime) + "h";
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
