#pragma once
#include "../Genetics/DNA.h"
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Gameplay {

// =========================================================================
// Binary Save Format
// =========================================================================
// Header: "MESO" magic + version + counts
// Entity records: fixed-size structs
// Park data: enclosures, buildings
// Economy snapshot
// =========================================================================

static constexpr uint32_t SAVE_MAGIC = 0x4F53454D; // "MESO"
static constexpr uint16_t SAVE_VERSION = 2;

#pragma pack(push, 1)

struct SaveHeader {
  uint32_t magic = SAVE_MAGIC;
  uint16_t version = SAVE_VERSION;
  uint16_t flags = 0;
  uint32_t entityCount = 0;
  uint32_t enclosureCount = 0;
  uint32_t buildingCount = 0;
  float gameTime = 0.0f;
  float balance = 0.0f;
  uint32_t day = 0;
  uint32_t reserved[4] = {};
};

struct SavedEntity {
  uint32_t id;
  uint32_t speciesId;
  float posX, posY, posZ;
  float health;
  float hunger;
  float thirst;
  float energy;
  float age;
  float sizeMultiplier;
  float speedMultiplier;
  float aggressionMultiplier;
  uint8_t dna[16]; // 128 bits of genome
  uint32_t enclosureId;
  uint8_t isPredator;
  uint8_t isAlive;
  uint8_t padding[2];
};

struct SavedEnclosure {
  uint32_t id;
  char name[32];
  float centerX, centerZ;
  float area;
  uint32_t fenceCount;
  uint8_t hasWater;
  uint8_t hasShelter;
  uint8_t padding[2];
};

struct SavedBuilding {
  uint32_t id;
  uint8_t type;
  uint8_t operational;
  uint8_t padding[2];
  float posX, posZ;
};

struct SavedEconomy {
  float balance;
  float totalIncome;
  float totalExpenses;
  float ticketPrice;
  float loanBalance;
  uint8_t hasInsurance;
  uint8_t padding[3];
};

#pragma pack(pop)

// =========================================================================
// Save/Load System
// =========================================================================

class SaveLoadSystem {
public:
  struct GameState {
    SaveHeader header;
    std::vector<SavedEntity> entities;
    std::vector<SavedEnclosure> enclosures;
    std::vector<SavedBuilding> buildings;
    SavedEconomy economy;
    bool valid = false;
  };

  // Save game to binary file
  static bool Save(const std::string &filepath, const GameState &state) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "[SaveLoad] Failed to open for writing: " << filepath
                << std::endl;
      return false;
    }

    // Write header
    SaveHeader header = state.header;
    header.magic = SAVE_MAGIC;
    header.version = SAVE_VERSION;
    header.entityCount = static_cast<uint32_t>(state.entities.size());
    header.enclosureCount = static_cast<uint32_t>(state.enclosures.size());
    header.buildingCount = static_cast<uint32_t>(state.buildings.size());

    file.write(reinterpret_cast<const char *>(&header), sizeof(SaveHeader));

    // Write entities
    for (const auto &e : state.entities) {
      file.write(reinterpret_cast<const char *>(&e), sizeof(SavedEntity));
    }

    // Write enclosures
    for (const auto &enc : state.enclosures) {
      file.write(reinterpret_cast<const char *>(&enc), sizeof(SavedEnclosure));
    }

    // Write buildings
    for (const auto &b : state.buildings) {
      file.write(reinterpret_cast<const char *>(&b), sizeof(SavedBuilding));
    }

    // Write economy
    file.write(reinterpret_cast<const char *>(&state.economy),
               sizeof(SavedEconomy));

    file.close();

    size_t totalBytes =
        sizeof(SaveHeader) + state.entities.size() * sizeof(SavedEntity) +
        state.enclosures.size() * sizeof(SavedEnclosure) +
        state.buildings.size() * sizeof(SavedBuilding) + sizeof(SavedEconomy);

    std::cout << "[SaveLoad] Saved to " << filepath << " (" << totalBytes
              << " bytes, " << state.entities.size() << " entities, "
              << state.enclosures.size() << " enclosures, "
              << state.buildings.size() << " buildings)" << std::endl;
    return true;
  }

  // Load game from binary file
  static GameState Load(const std::string &filepath) {
    GameState state;

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "[SaveLoad] File not found: " << filepath << std::endl;
      return state;
    }

    // Read header
    file.read(reinterpret_cast<char *>(&state.header), sizeof(SaveHeader));

    // Validate magic
    if (state.header.magic != SAVE_MAGIC) {
      std::cerr << "[SaveLoad] Invalid save file (bad magic number)"
                << std::endl;
      return state;
    }

    // Version check
    if (state.header.version > SAVE_VERSION) {
      std::cerr << "[SaveLoad] Save file version " << state.header.version
                << " is newer than supported " << SAVE_VERSION << std::endl;
      return state;
    }

    // Read entities
    state.entities.resize(state.header.entityCount);
    for (uint32_t i = 0; i < state.header.entityCount; i++) {
      file.read(reinterpret_cast<char *>(&state.entities[i]),
                sizeof(SavedEntity));
    }

    // Read enclosures
    state.enclosures.resize(state.header.enclosureCount);
    for (uint32_t i = 0; i < state.header.enclosureCount; i++) {
      file.read(reinterpret_cast<char *>(&state.enclosures[i]),
                sizeof(SavedEnclosure));
    }

    // Read buildings
    state.buildings.resize(state.header.buildingCount);
    for (uint32_t i = 0; i < state.header.buildingCount; i++) {
      file.read(reinterpret_cast<char *>(&state.buildings[i]),
                sizeof(SavedBuilding));
    }

    // Read economy
    file.read(reinterpret_cast<char *>(&state.economy), sizeof(SavedEconomy));

    state.valid = file.good();

    if (state.valid) {
      std::cout << "[SaveLoad] Loaded from " << filepath << ": "
                << state.entities.size() << " entities, Day "
                << state.header.day << ", $" << state.economy.balance
                << std::endl;
    } else {
      std::cerr << "[SaveLoad] Error reading save file" << std::endl;
    }

    return state;
  }

  // Convenience: convert genome to/from byte array
  static void GenomeToBytes(const Genetics::Genome &genome, uint8_t out[16]) {
    for (int i = 0; i < 128; i++) {
      int byteIdx = i / 8;
      int bitIdx = i % 8;
      if (genome.data[i])
        out[byteIdx] |= (1 << bitIdx);
      else
        out[byteIdx] &= ~(1 << bitIdx);
    }
  }

  static Genetics::Genome BytesToGenome(const uint8_t bytes[16]) {
    Genetics::Genome genome;
    for (int i = 0; i < 128; i++) {
      int byteIdx = i / 8;
      int bitIdx = i % 8;
      genome.data[i] = (bytes[byteIdx] >> bitIdx) & 1;
    }
    return genome;
  }

  // Auto-save slot management
  static std::string GetAutoSavePath(int slot = 0) {
    return "saves/autosave_" + std::to_string(slot) + ".meso";
  }

  static std::string GetManualSavePath(const std::string &name) {
    // 🛡️ Sentinel: Prevent Path Traversal
    // Sanitize input by extracting only the filename component.
    std::filesystem::path p(name);
    std::string safeName = p.filename().string();

    // Ensure we don't end up with empty or relative directory names
    if (safeName.empty() || safeName == "." || safeName == "..") {
      safeName = "unnamed_save";
    }
    return "saves/" + safeName + ".meso";
  }
};

} // namespace Gameplay
} // namespace Mesozoic
