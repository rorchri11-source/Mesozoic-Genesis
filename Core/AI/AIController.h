#pragma once
#include "UtilityCurves.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Core {
namespace AI {

struct Need {
  std::string name;
  float value;     // 0.0 = empty, 1.0 = full
  float decayRate; // Per second
  ResponseCurve urgencyCurve;

  float GetUrgency() const {
    // Urgency increases as value decreases (hunger = low -> urgent)
    return urgencyCurve.Evaluate(1.0f - value);
  }
};

enum class ActionType : uint8_t {
  Idle,
  Wander,
  SeekFood,
  Hunt,
  Eat,
  SeekWater,
  Drink,
  Flee,
  Sleep,
  Socialize,
  Defend,
  Patrol,
  COUNT
};

struct ActionScore {
  ActionType type;
  float score;
  uint32_t targetEntityId; // Relevant entity (prey, threat, etc.)
};

inline const char *ActionName(ActionType t) {
  switch (t) {
  case ActionType::Idle:
    return "Idle";
  case ActionType::Wander:
    return "Wander";
  case ActionType::SeekFood:
    return "SeekFood";
  case ActionType::Hunt:
    return "Hunt";
  case ActionType::Eat:
    return "Eat";
  case ActionType::SeekWater:
    return "SeekWater";
  case ActionType::Drink:
    return "Drink";
  case ActionType::Flee:
    return "Flee";
  case ActionType::Sleep:
    return "Sleep";
  case ActionType::Socialize:
    return "Socialize";
  case ActionType::Defend:
    return "Defend";
  case ActionType::Patrol:
    return "Patrol";
  default:
    return "Unknown";
  }
}

class AIController {
public:
  std::vector<Need> needs;
  ActionType currentAction = ActionType::Idle;
  uint32_t targetEntity = 0;
  bool isPredator = false;
  float aggressionLevel = 0.5f;

  void Initialize(bool predator, float aggression) {
    isPredator = predator;
    aggressionLevel = aggression;

    needs.clear();

    // Hunger: exponential urgency
    needs.push_back({"Hunger",
                     0.8f,
                     0.005f,
                     {CurveType::Exponential, 1.0f, 2.5f, 0.0f, 0.0f}});

    // Thirst: logistic urgency (sudden spike at low values)
    needs.push_back({"Thirst",
                     0.8f,
                     0.003f,
                     {CurveType::Logistic, 1.0f, 10.0f, 0.0f, 0.0f}});

    // Energy: linear urgency
    needs.push_back(
        {"Energy", 1.0f, 0.002f, {CurveType::Linear, 1.0f, 1.0f, 0.0f, 0.0f}});

    // Safety: sine curve (peaks at moderate danger)
    needs.push_back({"Safety",
                     1.0f,
                     0.0f,
                     {CurveType::Exponential, 1.0f, 3.0f, 0.0f, 0.0f}});
  }

  void UpdateNeeds(float dt) {
    for (auto &need : needs) {
      if (need.name == "Safety")
        continue; // Safety is set externally
      need.value -= need.decayRate * dt;
      need.value = std::clamp(need.value, 0.0f, 1.0f);
    }
  }

  void SetSafety(float safetyValue) {
    for (auto &need : needs) {
      if (need.name == "Safety") {
        need.value = safetyValue;
        return;
      }
    }
  }

  // Utility-based action selection: score all actions, pick highest
  ActionScore DecideAction(bool threatVisible, bool foodVisible,
                           bool waterNearby) {
    std::vector<ActionScore> scores;

    float hungerUrg = GetNeedUrgency("Hunger");
    float thirstUrg = GetNeedUrgency("Thirst");
    float energyUrg = GetNeedUrgency("Energy");
    float safetyUrg = GetNeedUrgency("Safety");

    // --- Flee (highest priority if threatened) ---
    if (threatVisible && !isPredator) {
      scores.push_back({ActionType::Flee, safetyUrg * 2.0f + 0.5f, 0});
    }

    // --- Hunt (predators only) ---
    if (isPredator && foodVisible && hungerUrg > 0.3f) {
      float huntScore = hungerUrg * aggressionLevel * 1.5f;
      scores.push_back({ActionType::Hunt, huntScore, 0});
    }

    // --- SeekFood ---
    if (hungerUrg > 0.2f) {
      float seekFoodScore = hungerUrg * (isPredator ? 0.8f : 1.2f);
      scores.push_back({ActionType::SeekFood, seekFoodScore, 0});
    }

    // --- Eat (when at food source) ---
    if (foodVisible && hungerUrg > 0.1f) {
      scores.push_back({ActionType::Eat, hungerUrg * 1.3f, 0});
    }

    // --- SeekWater ---
    if (thirstUrg > 0.2f) {
      scores.push_back({ActionType::SeekWater, thirstUrg * 1.1f, 0});
    }

    // --- Drink ---
    if (waterNearby && thirstUrg > 0.1f) {
      scores.push_back({ActionType::Drink, thirstUrg * 1.4f, 0});
    }

    // --- Sleep ---
    if (energyUrg > 0.6f) {
      scores.push_back({ActionType::Sleep, energyUrg * 0.9f, 0});
    }

    // --- Defend (not fleeing, but threatened) ---
    if (threatVisible && isPredator) {
      scores.push_back({ActionType::Defend, aggressionLevel * 0.7f, 0});
    }

    // --- Wander (default, always available) ---
    scores.push_back({ActionType::Wander, 0.1f, 0});

    // --- Idle ---
    scores.push_back({ActionType::Idle, 0.05f, 0});

    // Sort by score descending
    std::sort(scores.begin(), scores.end(),
              [](const ActionScore &a, const ActionScore &b) {
                return a.score > b.score;
              });

    currentAction = scores[0].type;
    return scores[0];
  }

  float GetNeedUrgency(const std::string &name) const {
    for (const auto &n : needs) {
      if (n.name == name)
        return n.GetUrgency();
    }
    return 0.0f;
  }

  float GetNeedValue(const std::string &name) const {
    for (const auto &n : needs) {
      if (n.name == name)
        return n.value;
    }
    return 0.0f;
  }

  void RestoreNeed(const std::string &name, float amount) {
    for (auto &n : needs) {
      if (n.name == name) {
        n.value = std::clamp(n.value + amount, 0.0f, 1.0f);
        return;
      }
    }
  }

  void SetNeedValue(const std::string &name, float value) {
    for (auto &n : needs) {
      if (n.name == name) {
        n.value = std::clamp(value, 0.0f, 1.0f);
        return;
      }
    }
  }
};

} // namespace AI
} // namespace Core
} // namespace Mesozoic
