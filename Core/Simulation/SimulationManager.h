#pragma once
#include "../../Graphics/TerrainSystem.h"
#include "../AI/AIController.h"
#include "../Math/Vec3.h"
#include "../Perception/SmellGrid.h"
#include "../Perception/VisionSystem.h"
#include "../Threading/JobSystem.h"
#include "EntityFactory.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>


namespace Mesozoic {
namespace Core {

class SimulationManager {
public:
  Graphics::TerrainSystem *terrainSystem = nullptr;

  std::vector<EntityFactory::DinosaurEntity> entities;
  std::vector<AI::AIController> aiControllers;
  Perception::SmellGrid smellGrid;
  Threading::JobSystem jobSystem;

  float simulationTime = 0.0f;
  int tickCount = 0;

  // Park stats
  int totalBirths = 0;
  int totalDeaths = 0;
  int predatorKills = 0;

  // Park environment
  std::array<float, 3> windDirection = {1.0f, 0.0f, 0.5f};
  float timeOfDay = 12.0f; // 0-24 hours
  bool isNight = false;

  // Water source positions
  std::vector<std::array<float, 3>> waterSources = {
      {0.0f, 0.0f, 0.0f}, {50.0f, 0.0f, 50.0f}, {-50.0f, 0.0f, -30.0f}};

  // Spawn a dinosaur
  uint32_t SpawnDinosaur(Species species) {
    Genetics::Genome dna;
    uint32_t seed = static_cast<uint32_t>(entities.size()) * 7919u + 42u;
    for (int i = 0; i < 20; ++i) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      bool p = (seed % 2) == 0;
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      bool m = (seed % 2) == 0;
      dna.SetLocus(static_cast<uint8_t>(i), p, m);
    }

    uint32_t id = static_cast<uint32_t>(entities.size());
    auto dino = EntityFactory::CreateDinosaur(id, species, dna);

    // Spread spawn positions
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    dino.transform.position[0] = static_cast<float>(seed % 200) - 100.0f;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    dino.transform.position[2] = static_cast<float>(seed % 200) - 100.0f;

    entities.push_back(dino);

    // Create AI controller
    AI::AIController ai;
    SpeciesData sp = GetSpeciesData(species);
    ai.Initialize(sp.isPredator, dino.genetics.aggressionLevel / 1.5f);
    aiControllers.push_back(ai);

    totalBirths++;
    return id;
  }

  // Breed two entities
  uint32_t Breed(uint32_t parent1Id, uint32_t parent2Id) {
    if (parent1Id >= entities.size() || parent2Id >= entities.size())
      return UINT32_MAX;
    auto &p1 = entities[parent1Id];
    auto &p2 = entities[parent2Id];
    if (!p1.vitals.alive || !p2.vitals.alive)
      return UINT32_MAX;
    if (p1.species != p2.species)
      return UINT32_MAX;

    uint32_t childId = static_cast<uint32_t>(entities.size());
    auto child = EntityFactory::Breed(childId, p1, p2);

    // Spawn near parents
    child.transform.position[0] =
        (p1.transform.position[0] + p2.transform.position[0]) * 0.5f + 5.0f;
    child.transform.position[2] =
        (p1.transform.position[2] + p2.transform.position[2]) * 0.5f + 5.0f;

    entities.push_back(child);

    AI::AIController ai;
    SpeciesData sp = GetSpeciesData(child.species);
    ai.Initialize(sp.isPredator, child.genetics.aggressionLevel / 1.5f);
    aiControllers.push_back(ai);

    totalBirths++;
    return childId;
  }

  // Main simulation tick
  void Tick(float dt) {
    simulationTime += dt;
    tickCount++;

    // Update time of day
    timeOfDay += dt / 60.0f; // 1 game minute = 1 real second
    if (timeOfDay >= 24.0f)
      timeOfDay -= 24.0f;
    isNight = (timeOfDay < 6.0f || timeOfDay > 20.0f);

    // Build perception data
    auto perceptionData = BuildPerceptionData();

    // Update each entity
    for (size_t i = 0; i < entities.size(); ++i) {
      auto &e = entities[i];
      if (!e.vitals.alive)
        continue;
      auto &ai = aiControllers[i];

      SpeciesData sp = GetSpeciesData(e.species);

      // 1. Update AI needs (hunger, thirst decay)
      ai.UpdateNeeds(dt);
      e.vitals.hunger = ai.GetNeedValue("Hunger") * 100.0f;
      e.vitals.thirst = ai.GetNeedValue("Thirst") * 100.0f;
      e.vitals.energy = ai.GetNeedValue("Energy") * 100.0f;
      e.vitals.age += dt;

      // 2. Vision: detect threats and food
      Perception::VisionSystem vision(sp.isPredator ? 55.0f : 160.0f, 80.0f);
      if (isNight)
        vision.nightPenalty = 0.4f;

      std::array<float, 3> forward = {std::cos(e.transform.rotation[1]), 0.0f,
                                      std::sin(e.transform.rotation[1])};
      auto visible = vision.ProcessVision(e.transform.position, forward,
                                          perceptionData, e.id);

      bool threatVisible = false;
      bool foodVisible = false;
      uint32_t nearestPreyId = UINT32_MAX;
      float nearestPreyDist = 9999.0f;

      for (const auto &v : visible) {
        if (v.isPredator && !sp.isPredator) {
          threatVisible = true;
          ai.SetSafety(std::max(0.0f, 1.0f - v.distance / 80.0f));
        }
        if (sp.isPredator && !v.isPredator && v.distance < nearestPreyDist) {
          foodVisible = true;
          nearestPreyId = v.entityId;
          nearestPreyDist = v.distance;
        }
      }
      if (!threatVisible)
        ai.SetSafety(1.0f);

      // 3. Check water proximity
      bool waterNearby = false;
      for (const auto &ws : waterSources) {
        float dx = e.transform.position[0] - ws[0];
        float dz = e.transform.position[2] - ws[2];
        if (dx * dx + dz * dz < 400.0f) { // Within 20 units
          waterNearby = true;
          break;
        }
      }

      // 4. AI Decision
      auto decision = ai.DecideAction(threatVisible, foodVisible, waterNearby);

      // 5. Execute action
      float speed = sp.baseSpeed * e.genetics.speedMultiplier;

      switch (decision.type) {
      case AI::ActionType::Wander: {
        // Random direction based on time
        float angle =
            std::sin(simulationTime * 0.1f + static_cast<float>(i) * 1.7f) *
            3.14159f;
        e.transform.position[0] += std::cos(angle) * speed * 0.3f * dt;
        e.transform.position[2] += std::sin(angle) * speed * 0.3f * dt;
        e.transform.rotation[1] = angle;
        ai.RestoreNeed("Energy", 0.001f * dt);
        break;
      }
      case AI::ActionType::Hunt: {
        if (nearestPreyId < entities.size()) {
          auto &prey = entities[nearestPreyId];
          float dx = prey.transform.position[0] - e.transform.position[0];
          float dz = prey.transform.position[2] - e.transform.position[2];
          float dist = std::sqrt(dx * dx + dz * dz);
          if (dist > 0.1f) {
            e.transform.position[0] += (dx / dist) * speed * dt;
            e.transform.position[2] += (dz / dist) * speed * dt;
            e.transform.rotation[1] = std::atan2(dz, dx);
          }
          // Attack if close enough
          if (dist < 5.0f && prey.vitals.alive) {
            float damage = 30.0f * e.genetics.sizeMultiplier *
                           e.genetics.aggressionLevel * dt;
            prey.vitals.health -= damage;
            if (prey.vitals.health <= 0.0f) {
              prey.vitals.alive = false;
              ai.RestoreNeed("Hunger", 0.6f);
              predatorKills++;
              std::cout << "  >> " << sp.name << " #" << e.id << " killed "
                        << GetSpeciesData(prey.species).name << " #" << prey.id
                        << "!" << std::endl;
            }
          }
        }
        break;
      }
      case AI::ActionType::Flee: {
        // Run away from threat
        for (const auto &v : visible) {
          if (v.isPredator && v.entityId < entities.size()) {
            auto &threat = entities[v.entityId];
            float dx = e.transform.position[0] - threat.transform.position[0];
            float dz = e.transform.position[2] - threat.transform.position[2];
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > 0.1f) {
              e.transform.position[0] += (dx / dist) * speed * 1.5f * dt;
              e.transform.position[2] += (dz / dist) * speed * 1.5f * dt;
            }
            break;
          }
        }
        break;
      }
      case AI::ActionType::SeekWater:
      case AI::ActionType::Drink: {
        // Move toward nearest water
        float nearestWaterDist = 9999.0f;
        size_t nearestWater = 0;
        for (size_t w = 0; w < waterSources.size(); w++) {
          float dx = waterSources[w][0] - e.transform.position[0];
          float dz = waterSources[w][2] - e.transform.position[2];
          float d = std::sqrt(dx * dx + dz * dz);
          if (d < nearestWaterDist) {
            nearestWaterDist = d;
            nearestWater = w;
          }
        }
        if (nearestWaterDist > 5.0f) {
          float dx = waterSources[nearestWater][0] - e.transform.position[0];
          float dz = waterSources[nearestWater][2] - e.transform.position[2];
          float d = std::sqrt(dx * dx + dz * dz);
          e.transform.position[0] += (dx / d) * speed * 0.8f * dt;
          e.transform.position[2] += (dz / d) * speed * 0.8f * dt;
        } else {
          ai.RestoreNeed("Thirst", 0.15f * dt);
        }
        break;
      }
      case AI::ActionType::SeekFood:
      case AI::ActionType::Eat: {
        if (!sp.isPredator) {
          // Herbivores graze anywhere
          ai.RestoreNeed("Hunger", 0.05f * dt);
        }
        break;
      }
      case AI::ActionType::Sleep: {
        ai.RestoreNeed("Energy", 0.1f * dt);
        break;
      }
      default:
        break;
      }

      e.aiState.currentGoal = static_cast<uint32_t>(decision.type);

      // 6. Starvation/dehydration damage
      if (ai.GetNeedValue("Hunger") <= 0.0f) {
        e.vitals.health -= 5.0f * dt;
      }
      if (ai.GetNeedValue("Thirst") <= 0.0f) {
        e.vitals.health -= 8.0f * dt;
      }

      // 7. Emit scent for smell grid
      smellGrid.EmitScent(e.transform.position, sp.isPredator ? 0.5f : 1.0f);

      // 8. Clamp position to world bounds
      e.transform.position[0] =
          std::clamp(e.transform.position[0], -768.0f, 768.0f);
      e.transform.position[2] =
          std::clamp(e.transform.position[2], -768.0f, 768.0f);

      // 9. Snap to Terrain
      if (terrainSystem) {
        e.transform.position[1] = terrainSystem->GetHeight(
            e.transform.position[0], e.transform.position[2]);
      } else {
        e.transform.position[1] = 0.0f;
      }
    }

    // Update smell grid
    smellGrid.Update(dt, windDirection);

    // Check deaths
    CheckDeaths();
  }

  // Print simulation status
  void PrintStatus() const {
    int alive = 0, dead = 0;
    int predators = 0, herbivores = 0;
    for (const auto &e : entities) {
      if (e.vitals.alive) {
        alive++;
        if (GetSpeciesData(e.species).isPredator)
          predators++;
        else
          herbivores++;
      } else
        dead++;
    }

    std::cout << "\n=====================================" << std::endl;
    std::cout << "  PARK STATUS | Time: " << static_cast<int>(simulationTime)
              << "s"
              << " | " << static_cast<int>(timeOfDay) << ":00"
              << (isNight ? " [NIGHT]" : " [DAY]") << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "  Alive: " << alive << " (" << predators << " predators, "
              << herbivores << " herbivores)" << std::endl;
    std::cout << "  Dead: " << dead << " | Births: " << totalBirths
              << " | Kills: " << predatorKills << std::endl;
    std::cout << "-------------------------------------" << std::endl;

    for (const auto &e : entities) {
      if (!e.vitals.alive)
        continue;
      SpeciesData sp = GetSpeciesData(e.species);
      const char *actionName =
          (e.aiState.currentGoal < static_cast<uint32_t>(AI::ActionType::COUNT))
              ? AI::ActionName(
                    static_cast<AI::ActionType>(e.aiState.currentGoal))
              : "?";

      std::cout << "  [" << e.id << "] " << sp.name
                << (sp.isPredator ? " *" : "  ")
                << " | HP:" << static_cast<int>(e.vitals.health)
                << " H:" << static_cast<int>(e.vitals.hunger)
                << " T:" << static_cast<int>(e.vitals.thirst)
                << " E:" << static_cast<int>(e.vitals.energy) << " | "
                << actionName << " | Pos("
                << static_cast<int>(e.transform.position[0]) << ","
                << static_cast<int>(e.transform.position[2]) << ")"
                << std::endl;
    }
  }

private:
  std::vector<Perception::EntityPerceptionData> BuildPerceptionData() const {
    std::vector<Perception::EntityPerceptionData> data;
    for (const auto &e : entities) {
      if (!e.vitals.alive)
        continue;
      Perception::EntityPerceptionData pd;
      pd.entityId = e.id;
      pd.position = Math::Vec3(e.transform.position);
      pd.radius = e.transform.scale[0]; // Size as radius
      pd.isPredator = GetSpeciesData(e.species).isPredator;
      pd.stealthFactor = 0.0f;
      data.push_back(pd);
    }
    return data;
  }

  void CheckDeaths() {
    for (auto &e : entities) {
      if (e.vitals.alive && e.vitals.health <= 0.0f) {
        e.vitals.alive = false;
        totalDeaths++;
        SpeciesData sp = GetSpeciesData(e.species);
        std::cout << "  >> " << sp.name << " #" << e.id
                  << " has died! (Age: " << static_cast<int>(e.vitals.age)
                  << "s)" << std::endl;
      }
    }
  }
};

} // namespace Core
} // namespace Mesozoic
