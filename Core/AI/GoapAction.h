#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace Mesozoic {
namespace Core {
namespace AI {

// Simple World State: "HasFood" -> 1, "TargetVisible" -> 1
using WorldState = std::unordered_map<std::string, int>;

class GoapAction {
public:
  std::string name;
  float cost = 1.0f;

  WorldState preconditions;
  WorldState effects;

  GoapAction(std::string name, float cost) : name(name), cost(cost) {}

  void AddPrecondition(std::string key, int value) {
    preconditions[key] = value;
  }

  void AddEffect(std::string key, int value) { effects[key] = value; }

  bool IsAchievable(const WorldState &currentEffects) {
    // Check if preconditions are met by the current state (or accumulated
    // effects)
    for (const auto &pre : preconditions) {
      // If state doesn't have the key, or value doesn't match
      if (currentEffects.find(pre.first) == currentEffects.end() ||
          currentEffects.at(pre.first) != pre.second) {
        return false;
      }
    }
    return true;
  }

  // Simulates applying this action's effects to a state
  void ApplyEffects(WorldState &state) {
    for (const auto &eff : effects) {
      state[eff.first] = eff.second;
    }
  }
};

} // namespace AI
} // namespace Core
} // namespace Mesozoic
