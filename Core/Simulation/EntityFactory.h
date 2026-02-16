#pragma once
#include "../../Genetics/DNA.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Core {

// Components attached to every dinosaur entity
struct TransformComponent {
  std::array<float, 3> position;
  std::array<float, 3> rotation;
  std::array<float, 3> scale;
};

struct VitalsComponent {
  float health;
  float hunger; // 0 = starving, 100 = full
  float thirst; // 0 = dehydrated, 100 = hydrated
  float energy; // 0 = exhausted, 100 = rested
  float age;    // seconds alive
  bool alive;
};

struct GeneticsComponent {
  Genetics::Genome dna;
  float sizeMultiplier;           // Derived from DNA
  float aggressionLevel;          // Derived from DNA
  float speedMultiplier;          // Derived from DNA
  std::array<float, 3> skinColor; // Derived from DNA
};

struct AIStateComponent {
  uint32_t currentGoal;   // GOAP goal ID
  uint32_t currentAction; // Current action in plan
  float actionProgress;   // 0-1 completion
  float decisionCooldown; // Seconds until next decision
};

enum class Species : uint8_t {
  TRex = 0,
  Velociraptor,
  Triceratops,
  Brachiosaurus,
  Stegosaurus,
  Pteranodon,
  Ankylosaurus,
  Parasaurolophus,
  COUNT
};

struct SpeciesData {
  std::string name;
  float baseHealth;
  float baseSpeed;
  float baseSize;
  bool isPredator;
  float hungerRate; // How fast hunger decreases per second
  float thirstRate;
};

// Predefined species stats
inline SpeciesData GetSpeciesData(Species s) {
  switch (s) {
  case Species::TRex:
    return {"T-Rex", 500.0f, 8.0f, 4.0f, true, 0.5f, 0.3f};
  case Species::Velociraptor:
    return {"Velociraptor", 150.0f, 15.0f, 1.0f, true, 0.8f, 0.5f};
  case Species::Triceratops:
    return {"Triceratops", 400.0f, 6.0f, 3.0f, false, 0.3f, 0.2f};
  case Species::Brachiosaurus:
    return {"Brachiosaurus", 800.0f, 4.0f, 8.0f, false, 0.2f, 0.15f};
  case Species::Stegosaurus:
    return {"Stegosaurus", 350.0f, 5.0f, 3.5f, false, 0.35f, 0.25f};
  case Species::Pteranodon:
    return {"Pteranodon", 100.0f, 20.0f, 1.5f, true, 0.7f, 0.4f};
  case Species::Ankylosaurus:
    return {"Ankylosaurus", 600.0f, 3.0f, 3.0f, false, 0.25f, 0.2f};
  case Species::Parasaurolophus:
    return {"Parasaurolophus", 250.0f, 9.0f, 3.0f, false, 0.4f, 0.3f};
  default:
    return {"Unknown", 100.0f, 5.0f, 1.0f, false, 0.5f, 0.5f};
  }
}

// Factory: creates a complete dinosaur entity from DNA + species
class EntityFactory {
public:
  struct DinosaurEntity {
    uint32_t id;
    Species species;
    TransformComponent transform;
    VitalsComponent vitals;
    GeneticsComponent genetics;
    AIStateComponent aiState;
  };

  static DinosaurEntity CreateDinosaur(uint32_t id, Species species,
                                       const Genetics::Genome &dna) {
    DinosaurEntity dino;
    dino.id = id;
    dino.species = species;

    SpeciesData base = GetSpeciesData(species);

    // Transform
    dino.transform.position = {0.0f, 0.0f, 0.0f};
    dino.transform.rotation = {0.0f, 0.0f, 0.0f};

    // Genetics -> physical traits
    float sizeTrait =
        Genetics::GeneticsEngine::ResolvePhenotype(dna.GetLocus(0));
    float speedTrait =
        Genetics::GeneticsEngine::ResolvePhenotype(dna.GetLocus(1));
    float aggrTrait =
        Genetics::GeneticsEngine::ResolvePhenotype(dna.GetLocus(2));

    dino.genetics.dna = dna;
    dino.genetics.sizeMultiplier = sizeTrait;
    dino.genetics.speedMultiplier = speedTrait;
    dino.genetics.aggressionLevel = aggrTrait;

    // Color from DNA loci 3-5
    float r =
        Genetics::GeneticsEngine::ResolvePhenotype(dna.GetLocus(3)) / 1.5f;
    float g =
        Genetics::GeneticsEngine::ResolvePhenotype(dna.GetLocus(4)) / 1.5f;
    float b =
        Genetics::GeneticsEngine::ResolvePhenotype(dna.GetLocus(5)) / 1.5f;
    dino.genetics.skinColor = {r, g, b};

    // Scale based on DNA
    float s = base.baseSize * sizeTrait;
    dino.transform.scale = {s, s, s};

    // Vitals
    dino.vitals.health = base.baseHealth * sizeTrait;
    dino.vitals.hunger = 80.0f;
    dino.vitals.thirst = 80.0f;
    dino.vitals.energy = 100.0f;
    dino.vitals.age = 0.0f;
    dino.vitals.alive = true;

    // AI
    dino.aiState.currentGoal = 0;
    dino.aiState.currentAction = 0;
    dino.aiState.actionProgress = 0.0f;
    dino.aiState.decisionCooldown = 0.0f;

    return dino;
  }

  // Breed two dinosaurs -> offspring (with unique seed for genetic diversity)
  static DinosaurEntity Breed(uint32_t childId, const DinosaurEntity &parent1,
                              const DinosaurEntity &parent2) {
    uint32_t seed =
        childId * 2654435761u + parent1.id * 7919u + parent2.id * 104729u;
    if (seed == 0)
      seed = 1;
    Genetics::Genome childDNA = Genetics::GeneticsEngine::Crossover(
        parent1.genetics.dna, parent2.genetics.dna, seed);
    return CreateDinosaur(childId, parent1.species, childDNA);
  }
};

} // namespace Core
} // namespace Mesozoic
