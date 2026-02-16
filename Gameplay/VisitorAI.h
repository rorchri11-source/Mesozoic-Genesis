#pragma once
#include "../Core/Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Gameplay {

using Math::Vec3;

// =========================================================================
// Visitor Needs & State
// =========================================================================

enum class VisitorMood : uint8_t {
  Ecstatic,
  Happy,
  Neutral,
  Unhappy,
  Angry,
  Terrified
};
enum class VisitorAction : uint8_t {
  Exploring,
  WatchingDinos,
  Eating,
  Shopping,
  Resting,
  Fleeing,
  Leaving
};

struct Visitor {
  uint32_t id;
  Vec3 position;
  Vec3 targetPosition;
  float speed = 1.5f;

  // Needs [0..1], lower = more urgent
  float hunger = 0.8f;
  float thirst = 0.8f;
  float energy = 1.0f;
  float excitement = 0.5f;
  float comfort = 0.7f;
  float fear = 0.0f;

  // State
  VisitorMood mood = VisitorMood::Neutral;
  VisitorAction action = VisitorAction::Exploring;
  float satisfaction = 0.5f; // Overall visit satisfaction [0..1]
  float moneySpent = 0.0f;
  float budget = 200.0f;       // How much they'll spend
  float timeInPark = 0.0f;     // Seconds
  float maxStayTime = 7200.0f; // 2 hours default

  // Species seen (for variety bonus)
  std::vector<uint32_t> speciesSeen;
};

// =========================================================================
// Visitor AI System
// =========================================================================

class VisitorAI {
  std::vector<Visitor> visitors;
  uint32_t nextVisitorId = 0;
  float spawnTimer = 0.0f;
  float spawnRate = 5.0f; // Seconds between spawns

public:
  uint32_t SpawnVisitor(Vec3 entrance) {
    Visitor v;
    v.id = nextVisitorId++;
    v.position = entrance;
    v.targetPosition = entrance;
    v.budget = 100.0f + static_cast<float>(v.id * 37 % 300);
    v.maxStayTime = 3600.0f + static_cast<float>(v.id * 53 % 7200);
    visitors.push_back(v);
    return v.id;
  }

  void Update(float dt, float parkRating, bool dinosaurEscaped) {
    spawnTimer += dt;

    // Spawn new visitors based on park rating
    float adjustedRate = spawnRate / (0.5f + parkRating);
    if (spawnTimer >= adjustedRate) {
      spawnTimer = 0;
      // Auto-spawn at default entrance
      SpawnVisitor(Vec3(0, 0, -200));
    }

    for (auto &v : visitors) {
      v.timeInPark += dt;

      // Decay needs
      v.hunger -= 0.0003f * dt;
      v.thirst -= 0.0005f * dt;
      v.energy -= 0.0002f * dt;
      v.excitement -= 0.0001f * dt;

      // Clamp
      v.hunger = std::clamp(v.hunger, 0.0f, 1.0f);
      v.thirst = std::clamp(v.thirst, 0.0f, 1.0f);
      v.energy = std::clamp(v.energy, 0.0f, 1.0f);
      v.excitement = std::clamp(v.excitement, 0.0f, 1.0f);

      // Escaped dinosaur = panic!
      if (dinosaurEscaped) {
        v.fear = std::min(1.0f, v.fear + 0.5f * dt);
        v.action = VisitorAction::Fleeing;
      } else {
        v.fear = std::max(0.0f, v.fear - 0.1f * dt);
      }

      // Decide action (utility-based)
      DecideAction(v);

      // Update mood
      UpdateMood(v);

      // Update satisfaction
      float moodFactor = (v.mood == VisitorMood::Ecstatic)  ? 1.0f
                         : (v.mood == VisitorMood::Happy)   ? 0.8f
                         : (v.mood == VisitorMood::Neutral) ? 0.5f
                         : (v.mood == VisitorMood::Unhappy) ? 0.3f
                         : (v.mood == VisitorMood::Angry)   ? 0.1f
                                                            : 0.0f;
      v.satisfaction = v.satisfaction * 0.99f + moodFactor * 0.01f;

      // Movement
      Vec3 delta = v.targetPosition - v.position;
      float dist = delta.Length();
      if (dist > 1.0f) {
        Vec3 dir = delta * (1.0f / dist);
        v.position = v.position + dir * (v.speed * dt);
      }
    }

    // Remove visitors who are leaving and have reached exit
    visitors.erase(std::remove_if(visitors.begin(), visitors.end(),
                                  [](const Visitor &v) {
                                    return v.action == VisitorAction::Leaving &&
                                           v.timeInPark > v.maxStayTime + 60.0f;
                                  }),
                   visitors.end());
  }

  // Handle dinosaur sighting
  void OnDinosaurSeen(uint32_t visitorId, uint32_t speciesId) {
    for (auto &v : visitors) {
      if (v.id == visitorId) {
        bool newSpecies = true;
        for (uint32_t s : v.speciesSeen) {
          if (s == speciesId) {
            newSpecies = false;
            break;
          }
        }
        if (newSpecies) {
          v.speciesSeen.push_back(speciesId);
          v.excitement = std::min(1.0f, v.excitement + 0.3f);
          v.satisfaction = std::min(1.0f, v.satisfaction + 0.1f);
        }
        v.excitement = std::min(1.0f, v.excitement + 0.05f);
        break;
      }
    }
  }

  // Stats
  size_t GetVisitorCount() const { return visitors.size(); }

  float GetAverageSatisfaction() const {
    if (visitors.empty())
      return 0;
    float total = 0;
    for (const auto &v : visitors)
      total += v.satisfaction;
    return total / visitors.size();
  }

  float GetTotalMoneySpent() const {
    float total = 0;
    for (const auto &v : visitors)
      total += v.moneySpent;
    return total;
  }

  int GetMoodCount(VisitorMood mood) const {
    int count = 0;
    for (const auto &v : visitors) {
      if (v.mood == mood)
        count++;
    }
    return count;
  }

  void PrintVisitorStats() const {
    std::cout << "\n=== VISITOR STATS ===" << std::endl;
    std::cout << "  Active: " << visitors.size() << std::endl;
    std::cout << "  Avg Satisfaction: "
              << static_cast<int>(GetAverageSatisfaction() * 100) << "%"
              << std::endl;
    std::cout << "  Mood Distribution:" << std::endl;
    std::cout << "    Ecstatic: " << GetMoodCount(VisitorMood::Ecstatic)
              << " | Happy: " << GetMoodCount(VisitorMood::Happy)
              << " | Neutral: " << GetMoodCount(VisitorMood::Neutral)
              << " | Unhappy: " << GetMoodCount(VisitorMood::Unhappy)
              << " | Angry: " << GetMoodCount(VisitorMood::Angry)
              << " | Terrified: " << GetMoodCount(VisitorMood::Terrified)
              << std::endl;
    std::cout << "  Total Spending: $" << static_cast<int>(GetTotalMoneySpent())
              << std::endl;
  }

private:
  void DecideAction(Visitor &v) {
    if (v.fear > 0.5f) {
      v.action = VisitorAction::Fleeing;
      return;
    }
    if (v.timeInPark > v.maxStayTime) {
      v.action = VisitorAction::Leaving;
      return;
    }

    // Utility scores
    float eatScore = (1.0f - v.hunger) * 2.0f;
    float restScore = (1.0f - v.energy) * 1.5f;
    float watchScore = (1.0f - v.excitement) * 1.2f + 0.3f;
    float shopScore = (v.budget - v.moneySpent > 20.0f) ? 0.4f : 0.0f;
    float exploreScore = 0.5f;

    // Pick highest scoring action
    float maxScore = exploreScore;
    v.action = VisitorAction::Exploring;

    if (eatScore > maxScore) {
      maxScore = eatScore;
      v.action = VisitorAction::Eating;
    }
    if (restScore > maxScore) {
      maxScore = restScore;
      v.action = VisitorAction::Resting;
    }
    if (watchScore > maxScore) {
      maxScore = watchScore;
      v.action = VisitorAction::WatchingDinos;
    }
    if (shopScore > maxScore) {
      maxScore = shopScore;
      v.action = VisitorAction::Shopping;
    }
  }

  void UpdateMood(Visitor &v) {
    if (v.fear > 0.5f) {
      v.mood = VisitorMood::Terrified;
      return;
    }

    float avg =
        (v.hunger + v.thirst + v.energy + v.excitement + v.comfort) / 5.0f;
    if (avg > 0.8f)
      v.mood = VisitorMood::Ecstatic;
    else if (avg > 0.6f)
      v.mood = VisitorMood::Happy;
    else if (avg > 0.4f)
      v.mood = VisitorMood::Neutral;
    else if (avg > 0.2f)
      v.mood = VisitorMood::Unhappy;
    else
      v.mood = VisitorMood::Angry;
  }
};

} // namespace Gameplay
} // namespace Mesozoic
