#pragma once
#include "GoapAction.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <vector>


namespace Mesozoic {
namespace Core {
namespace AI {

struct PlanNode {
  GoapAction *action;
  std::shared_ptr<PlanNode> parent;
  WorldState state;
  float g; // Cost so far
  float h; // Heuristic

  float f() const { return g + h; }
  bool operator>(const PlanNode &other) const { return f() > other.f(); }
};

class GoapPlanner {
public:
  // A* Forward Planning: find cheapest action sequence from start to goal
  static std::vector<GoapAction *>
  Plan(const WorldState &startState, const WorldState &goalState,
       std::vector<GoapAction *> &availableActions, int maxIterations = 500) {
    // Priority queue: lowest f() first
    auto cmp = [](const std::shared_ptr<PlanNode> &a,
                  const std::shared_ptr<PlanNode> &b) {
      return a->f() > b->f();
    };
    std::priority_queue<std::shared_ptr<PlanNode>,
                        std::vector<std::shared_ptr<PlanNode>>, decltype(cmp)>
        openSet(cmp);

    // Start node
    auto startNode = std::make_shared<PlanNode>();
    startNode->action = nullptr;
    startNode->parent = nullptr;
    startNode->state = startState;
    startNode->g = 0.0f;
    startNode->h = Heuristic(startState, goalState);
    openSet.push(startNode);

    int iterations = 0;

    while (!openSet.empty() && iterations < maxIterations) {
      iterations++;
      auto current = openSet.top();
      openSet.pop();

      // Check if goal is satisfied
      if (GoalMet(current->state, goalState)) {
        return ReconstructPlan(current);
      }

      // Expand: try each available action
      for (auto *action : availableActions) {
        if (action->IsAchievable(current->state)) {
          // Apply action effects to get new state
          WorldState newState = current->state;
          action->ApplyEffects(newState);

          auto child = std::make_shared<PlanNode>();
          child->action = action;
          child->parent = current;
          child->state = newState;
          child->g = current->g + action->cost;
          child->h = Heuristic(newState, goalState);

          openSet.push(child);
        }
      }
    }

    // No plan found
    return {};
  }

private:
  // Heuristic: count unsatisfied goal conditions
  static float Heuristic(const WorldState &current, const WorldState &goal) {
    float count = 0.0f;
    for (const auto &g : goal) {
      auto it = current.find(g.first);
      if (it == current.end() || it->second != g.second) {
        count += 1.0f;
      }
    }
    return count;
  }

  // Check if all goal conditions are met
  static bool GoalMet(const WorldState &current, const WorldState &goal) {
    for (const auto &g : goal) {
      auto it = current.find(g.first);
      if (it == current.end() || it->second != g.second) {
        return false;
      }
    }
    return true;
  }

  // Walk back through parent chain to build action sequence
  static std::vector<GoapAction *>
  ReconstructPlan(std::shared_ptr<PlanNode> node) {
    std::vector<GoapAction *> plan;
    while (node && node->action) {
      plan.push_back(node->action);
      node = node->parent;
    }
    std::reverse(plan.begin(), plan.end());
    return plan;
  }
};

} // namespace AI
} // namespace Core
} // namespace Mesozoic
