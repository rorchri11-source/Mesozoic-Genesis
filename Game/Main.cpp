#include "../Assets/GLTFLoader.h"
#include "../Assets/MorphTargetExtractor.h"
#include "../Assets/TextureLoader.h"
#include "../Core/Simulation/SimulationManager.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/TerrainGenerator.h"
#include "../Graphics/TerrainSystem.h"
#include "../Graphics/UI/UISystem.h"
#include "../Graphics/Window.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp> // Ensure GLM is included for vec3 math
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using namespace Mesozoic;
using namespace Mesozoic::Graphics;
using namespace Mesozoic::Core;
using namespace Mesozoic::Assets;

// Game State Enum
enum class GameState { MENU, PLAYING, EDITOR };

// Brush Modes
enum class BrushMode { NONE, RAISE, LOWER, FLATTEN };

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

  Renderer renderer;
  if (!renderer.Initialize(window, &backend)) {
    return -1;
  }

  // Initialize Terrain System (needs Renderer)
  TerrainSystem terrainSystem;
  terrainSystem.Initialize(&renderer, 512, 512, 3.0f, 50.0f);
  renderer.terrainSystem = &terrainSystem;

  // Initialize UI System (needs Backend & Window)
  UISystem uiSystem;
  uiSystem.Initialize(&backend, &window);
  renderer.uiSystem = &uiSystem;

  // Create UI Pipelines
  // Create UI Pipelines
  if (!backend.CreateUIPipeline("Shaders/ui.vert.spv", "Shaders/ui.frag.spv")) {
    std::cerr << "[Main] Failed to create UI Pipeline. UI might not render."
              << std::endl;
  }

  // Simulation
  SimulationManager sim;
  sim.terrainSystem = &terrainSystem;

  // --- ASSET LOADING ---

  // 1. White Texture for UI
  TextureLoader texLoader;
  std::vector<uint8_t> whitePx = {255, 255, 255, 255};
  GPUTexture whiteTex = backend.CreateTextureFromBuffer(
      whitePx.data(), whitePx.size(), 1, 1, VK_FORMAT_R8G8B8A8_UNORM);

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
    vert.uv = {v.uv[0], v.uv[1]};
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

  // --- MORPH SYSTEM SETUP ---
  // Generate Procedural Morphs for Dino
  auto morphSet =
      Assets::MorphTargetExtractor::GenerateDinosaurMorphs(gltfDino);

  // Flatten for GPU (Target_Snout, Target_Bulk, Target_Horn)
  struct ShaderMorphDelta {
    float p[4];
    float n[4];
  };
  std::vector<ShaderMorphDelta> morphData;
  size_t dinoVertCount = dinoMesh.baseVertices.size();
  morphData.resize(dinoVertCount * 3);

  // Helper to copy
  auto CopyTargetToBuffer = [&](const std::string &name, int targetIndex) {
    for (const auto &t : morphSet.targets) {
      if (t.name == name) {
        for (size_t i = 0; i < dinoVertCount && i < t.positionDeltas.size();
             ++i) {
          size_t dstIdx = targetIndex * dinoVertCount + i;
          morphData[dstIdx].p[0] = t.positionDeltas[i].x;
          morphData[dstIdx].p[1] = t.positionDeltas[i].y;
          morphData[dstIdx].p[2] = t.positionDeltas[i].z;
          morphData[dstIdx].p[3] = 0.0f; // Padding

          if (i < t.normalDeltas.size()) {
            morphData[dstIdx].n[0] = t.normalDeltas[i].x;
            morphData[dstIdx].n[1] = t.normalDeltas[i].y;
            morphData[dstIdx].n[2] = t.normalDeltas[i].z;
            morphData[dstIdx].n[3] = 0.0f;
          }
        }
        std::cout << "[Main] Bound Morph Target: " << name << " to index "
                  << targetIndex << std::endl;
        return;
      }
    }
    std::cerr << "[Main] Warning: Morph Target '" << name << "' not found!"
              << std::endl;
  };

  CopyTargetToBuffer("Target_Snout", 0);
  CopyTargetToBuffer("Target_Bulk", 1);
  CopyTargetToBuffer("Target_Horn", 2);

  // Upload to SSBO
  GPUBuffer morphBuffer =
      backend.CreateBuffer(morphData.size() * sizeof(ShaderMorphDelta),
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (morphBuffer.mapped) {
    memcpy(morphBuffer.mapped, morphData.data(),
           morphData.size() * sizeof(ShaderMorphDelta));
  }

  // Update Global Descriptors (Terrain + Morph)
  backend.UpdateDescriptorSets(terrainSystem.heightTex, terrainSystem.splatTex,
                               &morphBuffer);

  // Morph Weights UI State
  float wSnout = 0.0f;
  float wBulk = 0.0f;
  float wHorn = 0.0f;

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
      if (currentState == GameState::PLAYING ||
          currentState == GameState::EDITOR) {
        currentState = GameState::MENU;
        window.SetCursorLocked(false);
      } else {
        break;
      }
    }

    uiSystem.BeginFrame();
    renderer.renderQueue.clear();

    float screenW = uiSystem.GetScreenWidth();
    float screenH = uiSystem.GetScreenHeight();

    if (currentState == GameState::MENU) {

      uiSystem.DrawImage(0, 0, screenW, screenH, whiteTex,
                         {0.1f, 0.1f, 0.15f, 0.9f});

      float btnW = 300, btnH = 60;
      float centerX = (screenW - btnW) / 2;
      float centerY = (screenH) / 2 - 100;

      if (uiSystem.DrawButton(centerX, centerY, btnW, btnH, whiteTex,
                              {0.2f, 0.7f, 0.3f, 1.0f},
                              {0.3f, 0.9f, 0.4f, 1.0f})) {
        currentState = GameState::PLAYING;
        window.SetCursorLocked(true);
      }

      if (uiSystem.DrawButton(centerX, centerY + 80, btnW, btnH, whiteTex,
                              {0.2f, 0.5f, 0.8f, 1.0f},
                              {0.4f, 0.7f, 0.9f, 1.0f})) {
        currentState = GameState::EDITOR;
        window.SetCursorLocked(false); // Editor needs cursor
      }

      if (uiSystem.DrawButton(centerX, centerY + 160, btnW, btnH, whiteTex,
                              {0.7f, 0.2f, 0.2f, 1.0f},
                              {0.9f, 0.3f, 0.3f, 1.0f})) {
        break;
      }

    } else if (currentState == GameState::PLAYING ||
               currentState == GameState::EDITOR) {
      // --- GAMEPLAY/EDITOR LOGIC ---

      // 1. Simulation Tick
      if (!renderer.isDayCyclePaused) {
        sim.Tick(dt);
      }

      bool canMoveCamera = (currentState == GameState::PLAYING) ||
                           (currentState == GameState::EDITOR &&
                            window.IsMouseButtonDown(1)); // Right Click

      if (canMoveCamera) {
        float moveSpeed = 20.0f * dt;
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
        if (window.IsKeyPressed('E'))
          renderer.camera.position[1] += moveSpeed;
        if (window.IsKeyPressed('Q'))
          renderer.camera.position[1] -= moveSpeed;

        float dx, dy;
        window.GetMouseDelta(dx, dy);
        renderer.camera.Rotate(dx * 0.1f, dy * 0.1f);
      }

      // Terrain Constraint
      if (currentState == GameState::PLAYING) {
        float h = terrainSystem.GetHeight(renderer.camera.position.x,
                                          renderer.camera.position.z);
        if (renderer.camera.position.y < h + 2.0f)
          renderer.camera.position.y = h + 2.0f;
      }

      // --- EDITOR LOGIC ---
      if (currentState == GameState::EDITOR) {
        float mx, my;
        window.GetMousePosition(mx, my);

        float panelW = 300.0f;
        bool mouseOverUI = (mx > (screenW - panelW));

        if (!mouseOverUI && window.IsMouseButtonDown(0)) {
          // Mouse Picking
          float ndcX = (mx / screenW) * 2.0f - 1.0f;
          float ndcY = (my / screenH) * 2.0f - 1.0f;
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
          Vec3 rayOrigin(renderer.camera.position.x, renderer.camera.position.y,
                         renderer.camera.position.z);

          float hitT;
          Vec3 hitPos;
          if (terrainSystem.Raycast(rayOrigin, rayDir, hitT, hitPos)) {
            if (brushMode != BrushMode::NONE) {
              // Height Modification
              if (brushMode == BrushMode::FLATTEN) {
                if (!isFlattenTargetSet) {
                  flattenTargetHeight = hitPos.y;
                  isFlattenTargetSet = true;
                }
              } else {
                isFlattenTargetSet = false;
              }

              int modeInt = (brushMode == BrushMode::RAISE)
                                ? 0
                                : (brushMode == BrushMode::LOWER ? 1 : 2);
              terrainSystem.ModifyHeight(hitPos.x, hitPos.z, brushSize,
                                         brushStrength * dt, modeInt,
                                         flattenTargetHeight);
            } else {
              // Texture Painting
              terrainSystem.Paint(hitPos.x, hitPos.z, brushRadius, brushType);
            }
          }
        } else if (!window.IsMouseButtonDown(0)) {
          isFlattenTargetSet = false;
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
           .meshIndex = terrainSystem.meshId,
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

        // Apply Morph Weights
        obj.morphWeights = {wSnout, wBulk, wHorn, 0.0f};

        renderer.SubmitEntity(obj);
      }

      renderer.SubmitEntity(
          {.entityId = 50000,
           .meshIndex = grassMeshId,
           .worldTransform = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
           .color = {0.1f, 0.8f, 0.2f, 0.5f},
           .visible = true});

      // 5. UI Overlay
      std::stringstream ss_ui;
      ss_ui << (renderer.isDayCyclePaused ? "[PAUSED] " : "")
            << "Time: " << std::fixed << std::setprecision(1)
            << renderer.dayTime << "h";

      if (currentState == GameState::EDITOR) {
        // Editor Panel Background
        float panelW = 300;
        uiSystem.DrawImage(screenW - panelW, 0, panelW, screenH, whiteTex,
                           {0.2f, 0.2f, 0.2f, 0.8f});

        float x = screenW - panelW + 20;
        float y = 50;

        // Day/Night Controls
        if (uiSystem.DrawButton(x, y, 40, 30, whiteTex, {0.4f, 0.4f, 0.4f, 1},
                                {0.5f, 0.5f, 0.5f, 1})) {
          renderer.daySpeed = std::max(0.0f, renderer.daySpeed - 0.1f);
        }
        if (uiSystem.DrawButton(x + 50, y, 40, 30, whiteTex,
                                {0.4f, 0.4f, 0.4f, 1}, {0.5f, 0.5f, 0.5f, 1})) {
          renderer.daySpeed += 0.1f;
        }
        if (uiSystem.DrawButton(x + 100, y, 80, 30, whiteTex,
                                renderer.isDayCyclePaused
                                    ? glm::vec4(0.8, 0.2, 0.2, 1)
                                    : glm::vec4(0.2, 0.8, 0.2, 1),
                                {0.5f, 0.5f, 0.5f, 1})) {
          renderer.isDayCyclePaused = !renderer.isDayCyclePaused;
        }
        y += 50;

        // Time Slider
        float sliderW = 200;
        uiSystem.DrawImage(x, y, sliderW, 10, whiteTex, {0, 0, 0, 1});
        float knobX = x + (renderer.dayTime / 24.0f) * sliderW;
        uiSystem.DrawImage(knobX - 5, y - 5, 10, 20, whiteTex, {1, 1, 0, 1});

        float mx, my;
        window.GetMousePosition(mx, my);
        if (window.IsMouseButtonDown(0) && mx >= x && mx <= x + sliderW &&
            my >= y - 10 && my <= y + 20) {
          renderer.dayTime = ((mx - x) / sliderW) * 24.0f;
        }
        y += 50;

        // Brush Modes
        auto drawModeBtn = [&](const std::string &name, BrushMode mode,
                               float offsetY) {
          bool active = (brushMode == mode);
          glm::vec4 col = active ? glm::vec4(0.3, 0.8, 0.3, 1)
                                 : glm::vec4(0.4, 0.4, 0.4, 1);
          if (uiSystem.DrawButton(x, y + offsetY, 200, 30, whiteTex, col,
                                  {0.5, 0.5, 0.5, 1})) {
            brushMode = mode;
          }
        };

        drawModeBtn("Raise", BrushMode::RAISE, 0);
        drawModeBtn("Lower", BrushMode::LOWER, 40);
        drawModeBtn("Flatten", BrushMode::FLATTEN, 80);
        drawModeBtn("None", BrushMode::NONE, 120);
        y += 180;

        // Brush Size
        uiSystem.DrawImage(x, y, sliderW, 10, whiteTex, {0, 0, 0, 1});
        float sizeKnobX = x + (brushSize / 50.0f) * sliderW;
        uiSystem.DrawImage(sizeKnobX - 5, y - 5, 10, 20, whiteTex,
                           {0.8, 0.8, 1, 1});
        if (window.IsMouseButtonDown(0) && mx >= x && mx <= x + sliderW &&
            my >= y - 10 && my <= y + 20) {
          brushSize = std::max(1.0f, ((mx - x) / sliderW) * 50.0f);
        }
        y += 40;

        // Brush Strength
        uiSystem.DrawImage(x, y, sliderW, 10, whiteTex, {0, 0, 0, 1});
        float strKnobX = x + (brushStrength / 20.0f) * sliderW;
        uiSystem.DrawImage(strKnobX - 5, y - 5, 10, 20, whiteTex,
                           {1, 0.5, 0.5, 1});
        if (window.IsMouseButtonDown(0) && mx >= x && mx <= x + sliderW &&
            my >= y - 10 && my <= y + 20) {
          brushStrength = ((mx - x) / sliderW) * 20.0f;
        }

        // Morph Sliders Panel
        float mPanelW = 250, mPanelH = 150;
        float mPanelX = screenW - mPanelW - 310;
        float mPanelY = screenH - mPanelH - 20;
        uiSystem.DrawImage(mPanelX, mPanelY, mPanelW, mPanelH, whiteTex,
                           {0.0f, 0.0f, 0.0f, 0.5f});

        float sX = mPanelX + 10, sY = mPanelY + 10, sW = mPanelW - 20, sH = 20,
              gap = 30;
        uiSystem.DrawSlider(sX, sY, sW, sH, wSnout, whiteTex,
                            {0.5, 0.2, 0.2, 1}, {1, 0.5, 0.5, 1});
        uiSystem.DrawSlider(sX, sY + gap, sW, sH, wBulk, whiteTex,
                            {0.2, 0.5, 0.2, 1}, {0.5, 1, 0.5, 1});
        uiSystem.DrawSlider(sX, sY + gap * 2, sW, sH, wHorn, whiteTex,
                            {0.2, 0.2, 0.5, 1}, {0.5, 0.5, 1, 1});
      }

      // --- IN-GAME UI ---
      float editorBtnW = 100;
      if (uiSystem.DrawButton(screenW - editorBtnW - 10, 10, editorBtnW, 40,
                              whiteTex, {0.6f, 0.6f, 1.0f, 0.8f},
                              {0.8f, 0.8f, 1.0f, 1.0f})) {
        static auto lastToggleUI = std::chrono::high_resolution_clock::now();
        auto nowUI = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(nowUI -
                                                                  lastToggleUI)
                .count() > 500) {
          if (currentState == GameState::EDITOR)
            currentState = GameState::PLAYING;
          else if (currentState == GameState::PLAYING)
            currentState = GameState::EDITOR;
          window.SetCursorLocked(currentState == GameState::PLAYING);
          lastToggleUI = nowUI;
        }
      }
    }

    renderer.camera.aspectRatio = (float)config.width / (float)config.height;

    Vec3 camPos(renderer.camera.position.x, renderer.camera.position.y,
                renderer.camera.position.z);
    Vec3 camFwd(renderer.camera.GetForward().x, renderer.camera.GetForward().y,
                renderer.camera.GetForward().z);
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
      std::string title =
          config.title + " | FPS: " + std::to_string(frameCount) +
          " | Ents: " + std::to_string(sim.entities.size()) +
          " | Time: " + std::to_string((int)renderer.dayTime) + "h";
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
