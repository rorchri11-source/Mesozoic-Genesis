#pragma once
#include "../Core/Math/Vec3.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mesozoic {
namespace Gameplay {

using Math::Vec3;

// =========================================================================
// Enclosure & Infrastructure
// =========================================================================

enum class FenceType : uint8_t {
  WoodFence,       // Cheap, low security
  ChainLink,       // Medium cost/security
  ElectricFence,   // Expensive, high security
  ReinforcedSteel, // Maximum security for large predators
  InvisibleBarrier // Premium — glass walls
};

struct FenceSegment {
  Vec3 start, end;
  FenceType type;
  float health = 100.0f; // Degrades over time or from attacks
  float maxHealth = 100.0f;
  float securityRating = 0.5f;

  float Length() const { return (end - start).Length(); }
  float CostPerMeter() const {
    switch (type) {
    case FenceType::WoodFence:
      return 50.0f;
    case FenceType::ChainLink:
      return 120.0f;
    case FenceType::ElectricFence:
      return 300.0f;
    case FenceType::ReinforcedSteel:
      return 600.0f;
    case FenceType::InvisibleBarrier:
      return 1200.0f;
    }
    return 100.0f;
  }
};

struct Enclosure {
  uint32_t id;
  std::string name;
  std::vector<FenceSegment> fences;
  std::vector<uint32_t> dinosaurIds;
  Vec3 center;
  float area = 0.0f;
  float comfortRating = 0.5f; // Affects dino happiness
  bool hasWater = false;
  bool hasShelter = false;

  // Calculate enclosure stats
  void UpdateStats() {
    if (fences.size() < 3) {
      area = 0;
      return;
    }

    // Approximate area via shoelace formula on fence endpoints
    float a = 0.0f;
    for (size_t i = 0; i < fences.size(); i++) {
      const auto &p1 = fences[i].start;
      const auto &p2 = fences[(i + 1) % fences.size()].start;
      a += p1.x * p2.z - p2.x * p1.z;
    }
    area = std::abs(a) * 0.5f;

    // Center of mass
    center = Vec3(0, 0, 0);
    for (const auto &f : fences) {
      center = center + f.start;
    }
    center = center * (1.0f / fences.size());

    // Comfort rating based on features
    float spacePer =
        area / std::max(1u, static_cast<uint32_t>(dinosaurIds.size()));
    comfortRating = std::clamp(spacePer / 500.0f, 0.0f, 1.0f);
    if (hasWater)
      comfortRating += 0.15f;
    if (hasShelter)
      comfortRating += 0.1f;

    float minFenceHealth = 1.0f;
    for (const auto &f : fences) {
      minFenceHealth = std::min(minFenceHealth, f.health / f.maxHealth);
    }
    comfortRating = std::clamp(comfortRating, 0.0f, 1.0f);
  }

  float GetSecurityRating() const {
    if (fences.empty())
      return 0.0f;
    float minSec = 1.0f;
    for (const auto &f : fences) {
      minSec = std::min(minSec, (f.health / f.maxHealth) * f.securityRating);
    }
    return minSec;
  }

  float GetTotalFenceCost() const {
    float cost = 0;
    for (const auto &f : fences)
      cost += f.Length() * f.CostPerMeter();
    return cost;
  }
};

// =========================================================================
// Buildings & Facilities
// =========================================================================

enum class BuildingType : uint8_t {
  // Guest facilities
  VisitorCenter,
  Restaurant,
  GiftShop,
  Restroom,
  ViewingPlatform,
  // Operations
  ResearchLab,
  HatcheryLab,
  VetClinic,
  PowerStation,
  MaintenanceDepot,
  // Attractions
  GyrosphereStation,
  FeedingShowArena
};

struct Building {
  uint32_t id;
  BuildingType type;
  std::string name;
  Vec3 position;
  float constructionCost;
  float maintenanceCost; // Per tick
  float revenue;         // Per visitor per tick
  float capacity;        // Max visitors
  float satisfaction;    // How much it boosts visitor satisfaction
  bool operational = true;

  static Building Create(BuildingType type, Vec3 pos) {
    Building b;
    b.type = type;
    b.position = pos;
    b.operational = true;

    switch (type) {
    case BuildingType::VisitorCenter:
      b.name = "Visitor Center";
      b.constructionCost = 50000;
      b.maintenanceCost = 200;
      b.revenue = 15;
      b.capacity = 100;
      b.satisfaction = 0.1f;
      break;
    case BuildingType::Restaurant:
      b.name = "Restaurant";
      b.constructionCost = 30000;
      b.maintenanceCost = 150;
      b.revenue = 25;
      b.capacity = 50;
      b.satisfaction = 0.2f;
      break;
    case BuildingType::GiftShop:
      b.name = "Gift Shop";
      b.constructionCost = 15000;
      b.maintenanceCost = 80;
      b.revenue = 35;
      b.capacity = 30;
      b.satisfaction = 0.1f;
      break;
    case BuildingType::Restroom:
      b.name = "Restroom";
      b.constructionCost = 5000;
      b.maintenanceCost = 50;
      b.revenue = 0;
      b.capacity = 20;
      b.satisfaction = 0.15f;
      break;
    case BuildingType::ViewingPlatform:
      b.name = "Viewing Platform";
      b.constructionCost = 25000;
      b.maintenanceCost = 100;
      b.revenue = 10;
      b.capacity = 40;
      b.satisfaction = 0.3f;
      break;
    case BuildingType::ResearchLab:
      b.name = "Research Lab";
      b.constructionCost = 100000;
      b.maintenanceCost = 500;
      b.revenue = 0;
      b.capacity = 10;
      b.satisfaction = 0.05f;
      break;
    case BuildingType::HatcheryLab:
      b.name = "Hatchery Lab";
      b.constructionCost = 80000;
      b.maintenanceCost = 400;
      b.revenue = 0;
      b.capacity = 5;
      b.satisfaction = 0.05f;
      break;
    case BuildingType::VetClinic:
      b.name = "Vet Clinic";
      b.constructionCost = 60000;
      b.maintenanceCost = 300;
      b.revenue = 0;
      b.capacity = 5;
      b.satisfaction = 0.0f;
      break;
    case BuildingType::PowerStation:
      b.name = "Power Station";
      b.constructionCost = 40000;
      b.maintenanceCost = 250;
      b.revenue = 0;
      b.capacity = 0;
      b.satisfaction = 0.0f;
      break;
    case BuildingType::MaintenanceDepot:
      b.name = "Maintenance Depot";
      b.constructionCost = 20000;
      b.maintenanceCost = 100;
      b.revenue = 0;
      b.capacity = 0;
      b.satisfaction = 0.0f;
      break;
    case BuildingType::GyrosphereStation:
      b.name = "Gyrosphere Station";
      b.constructionCost = 75000;
      b.maintenanceCost = 350;
      b.revenue = 50;
      b.capacity = 20;
      b.satisfaction = 0.4f;
      break;
    case BuildingType::FeedingShowArena:
      b.name = "Feeding Show Arena";
      b.constructionCost = 90000;
      b.maintenanceCost = 400;
      b.revenue = 40;
      b.capacity = 80;
      b.satisfaction = 0.35f;
      break;
    }
    return b;
  }
};

// =========================================================================
// Park Manager
// =========================================================================

class ParkManager {
  std::vector<Enclosure> enclosures;
  std::vector<Building> buildings;
  uint32_t nextEnclosureId = 0;
  uint32_t nextBuildingId = 0;

public:
  // --- Enclosure Management ---
  uint32_t CreateEnclosure(const std::string &name) {
    Enclosure enc;
    enc.id = nextEnclosureId++;
    enc.name = name;
    enclosures.push_back(enc);
    std::cout << "[Park] Created enclosure: " << name << " (ID " << enc.id
              << ")" << std::endl;
    return enc.id;
  }

  bool AddFence(uint32_t enclosureId, Vec3 start, Vec3 end, FenceType type) {
    auto *enc = FindEnclosure(enclosureId);
    if (!enc)
      return false;

    FenceSegment seg;
    seg.start = start;
    seg.end = end;
    seg.type = type;
    seg.securityRating = static_cast<float>(type) / 4.0f + 0.2f;
    seg.maxHealth = 100.0f + static_cast<float>(type) * 50.0f;
    seg.health = seg.maxHealth;
    enc->fences.push_back(seg);
    enc->UpdateStats();
    return true;
  }

  bool AddDinosaurToEnclosure(uint32_t enclosureId, uint32_t dinoId) {
    auto *enc = FindEnclosure(enclosureId);
    if (!enc)
      return false;
    enc->dinosaurIds.push_back(dinoId);
    enc->UpdateStats();
    return true;
  }

  // --- Building Management ---
  uint32_t PlaceBuilding(BuildingType type, Vec3 position) {
    auto b = Building::Create(type, position);
    b.id = nextBuildingId++;
    std::cout << "[Park] Placed " << b.name << " at (" << position.x << ", "
              << position.z << ") — Cost: $" << b.constructionCost << std::endl;
    buildings.push_back(b);
    return b.id;
  }

  // --- Queries ---
  float GetTotalMaintenanceCost() const {
    float cost = 0;
    for (const auto &b : buildings) {
      if (b.operational)
        cost += b.maintenanceCost;
    }
    return cost;
  }

  float GetTotalRevenue(uint32_t visitorCount) const {
    float rev = 0;
    for (const auto &b : buildings) {
      if (b.operational) {
        uint32_t served = std::min(
            static_cast<uint32_t>(b.capacity),
            visitorCount / static_cast<uint32_t>(std::max(
                               1, static_cast<int>(buildings.size()))));
        rev += b.revenue * served;
      }
    }
    return rev;
  }

  float GetParkRating() const {
    if (enclosures.empty() && buildings.empty())
      return 0.0f;

    float dinoScore = 0;
    size_t totalDinos = 0;
    for (const auto &enc : enclosures) {
      dinoScore += enc.comfortRating * enc.dinosaurIds.size();
      totalDinos += enc.dinosaurIds.size();
    }
    float dinoAvg = totalDinos > 0 ? dinoScore / totalDinos : 0;

    float facilityScore = 0;
    for (const auto &b : buildings) {
      if (b.operational)
        facilityScore += b.satisfaction;
    }
    facilityScore = std::min(facilityScore, 1.0f);

    float speciesBonus = static_cast<float>(enclosures.size()) * 0.05f;

    return std::clamp((dinoAvg * 0.4f + facilityScore * 0.4f + speciesBonus) *
                          5.0f,
                      0.0f, 5.0f);
  }

  size_t GetEnclosureCount() const { return enclosures.size(); }
  size_t GetBuildingCount() const { return buildings.size(); }
  const std::vector<Enclosure> &GetEnclosures() const { return enclosures; }
  const std::vector<Building> &GetBuildings() const { return buildings; }

  void PrintParkStatus() const {
    std::cout << "\n=== PARK STATUS ===" << std::endl;
    std::cout << "  Rating: " << GetParkRating() << "/5.0 stars" << std::endl;
    std::cout << "  Enclosures: " << enclosures.size() << std::endl;
    for (const auto &enc : enclosures) {
      std::cout << "    [" << enc.name << "] Dinos: " << enc.dinosaurIds.size()
                << " | Area: " << enc.area
                << "m² | Comfort: " << static_cast<int>(enc.comfortRating * 100)
                << "%"
                << " | Security: "
                << static_cast<int>(enc.GetSecurityRating() * 100) << "%"
                << std::endl;
    }
    std::cout << "  Buildings: " << buildings.size() << std::endl;
    for (const auto &b : buildings) {
      std::cout << "    [" << b.name << "] " << (b.operational ? "OK" : "DOWN")
                << " | Revenue: $" << b.revenue << "/visitor" << std::endl;
    }
    std::cout << "  Total Maintenance: $" << GetTotalMaintenanceCost()
              << "/tick" << std::endl;
  }

private:
  Enclosure *FindEnclosure(uint32_t id) {
    for (auto &e : enclosures) {
      if (e.id == id)
        return &e;
    }
    return nullptr;
  }
};

} // namespace Gameplay
} // namespace Mesozoic
