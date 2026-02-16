#pragma once
#include "../Core/Math/Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Assets {

using Math::Quat;
using Math::Vec3;

// =========================================================================
// Animation Clip Data Structures
// =========================================================================

struct BoneKeyframe {
  float time;
  Vec3 translation;
  Quat rotation;
  Vec3 scale;
};

struct BoneTrack {
  int boneIndex;
  std::string boneName;
  std::vector<BoneKeyframe> keyframes;

  // Interpolate bone transform at time t
  BoneKeyframe Sample(float t) const {
    if (keyframes.empty())
      return {};
    if (keyframes.size() == 1 || t <= keyframes.front().time)
      return keyframes.front();
    if (t >= keyframes.back().time)
      return keyframes.back();

    // Find surrounding keyframes
    size_t i = 0;
    for (; i < keyframes.size() - 1; i++) {
      if (t < keyframes[i + 1].time)
        break;
    }

    const auto &a = keyframes[i];
    const auto &b = keyframes[i + 1];
    float alpha = (t - a.time) / (b.time - a.time);

    BoneKeyframe result;
    result.time = t;
    // Lerp translation and scale
    result.translation =
        a.translation + (b.translation - a.translation) * alpha;
    result.scale = a.scale + (b.scale - a.scale) * alpha;
    // Slerp rotation
    result.rotation = Quat::Slerp(a.rotation, b.rotation, alpha);
    return result;
  }
};

struct AnimationClip {
  std::string name;
  float duration = 0.0f;
  float ticksPerSecond = 30.0f;
  std::vector<BoneTrack> tracks;
  bool loops = true;

  // Sample all bone transforms at time t
  std::vector<BoneKeyframe> SampleAll(float t) const {
    float clampedT = t;
    if (loops && duration > 0) {
      clampedT = std::fmod(t, duration);
      if (clampedT < 0)
        clampedT += duration;
    } else {
      clampedT = std::clamp(t, 0.0f, duration);
    }

    std::vector<BoneKeyframe> result;
    result.reserve(tracks.size());
    for (const auto &track : tracks) {
      result.push_back(track.Sample(clampedT));
    }
    return result;
  }
};

// =========================================================================
// Skeleton Definition
// =========================================================================

struct Bone {
  std::string name;
  int parentIndex = -1;
  Vec3 localPosition;
  Quat localRotation;
  Vec3 localScale = Vec3(1, 1, 1);
  std::array<float, 16> inverseBindMatrix;
};

struct Skeleton {
  std::string name;
  std::vector<Bone> bones;
  int rootBoneIndex = 0;

  int FindBone(const std::string &boneName) const {
    for (size_t i = 0; i < bones.size(); i++) {
      if (bones[i].name == boneName)
        return static_cast<int>(i);
    }
    return -1;
  }
};

// =========================================================================
// Animation Loader
// =========================================================================

class AnimationLoader {
public:
  // Create a procedural walk cycle for a dinosaur skeleton
  static AnimationClip CreateWalkCycle(const Skeleton &skeleton,
                                       float duration = 1.0f) {
    AnimationClip clip;
    clip.name = "walk_cycle";
    clip.duration = duration;
    clip.loops = true;

    for (size_t i = 0; i < skeleton.bones.size(); i++) {
      BoneTrack track;
      track.boneIndex = static_cast<int>(i);
      track.boneName = skeleton.bones[i].name;

      int keyCount = 5;
      for (int k = 0; k < keyCount; k++) {
        float t = (static_cast<float>(k) / (keyCount - 1)) * duration;
        float phase = (t / duration) * 2.0f * 3.14159f;

        BoneKeyframe kf;
        kf.time = t;
        kf.translation = skeleton.bones[i].localPosition;
        kf.rotation = skeleton.bones[i].localRotation;
        kf.scale = skeleton.bones[i].localScale;

        // Apply walk motion based on bone role (detected by name)
        const auto &name = track.boneName;
        if (name.find("Hip") != std::string::npos ||
            name.find("Root") != std::string::npos) {
          kf.translation.y += std::sin(phase * 2.0f) * 0.05f; // Bounce
        }
        if (name.find("Leg") != std::string::npos ||
            name.find("Thigh") != std::string::npos) {
          float legPhase = (name.find("Left") != std::string::npos ||
                            name.find("_L") != std::string::npos)
                               ? phase
                               : phase + 3.14159f;
          float angle = std::sin(legPhase) * 0.5f;
          kf.rotation = Quat::FromAxisAngle(Vec3(1, 0, 0), angle) * kf.rotation;
        }
        if (name.find("Tail") != std::string::npos) {
          float angle = std::sin(phase + 1.0f) * 0.15f;
          kf.rotation = Quat::FromAxisAngle(Vec3(0, 1, 0), angle) * kf.rotation;
        }
        if (name.find("Head") != std::string::npos ||
            name.find("Neck") != std::string::npos) {
          float angle = std::sin(phase * 0.5f) * 0.08f;
          kf.rotation = Quat::FromAxisAngle(Vec3(0, 1, 0), angle) * kf.rotation;
        }

        track.keyframes.push_back(kf);
      }
      clip.tracks.push_back(track);
    }

    std::cout << "[AnimationLoader] Created walk cycle: " << clip.tracks.size()
              << " bone tracks, " << duration << "s duration" << std::endl;
    return clip;
  }

  // Create idle animation (subtle breathing)
  static AnimationClip CreateIdleAnim(const Skeleton &skeleton,
                                      float duration = 3.0f) {
    AnimationClip clip;
    clip.name = "idle";
    clip.duration = duration;
    clip.loops = true;

    for (size_t i = 0; i < skeleton.bones.size(); i++) {
      BoneTrack track;
      track.boneIndex = static_cast<int>(i);
      track.boneName = skeleton.bones[i].name;

      for (int k = 0; k < 4; k++) {
        float t = (static_cast<float>(k) / 3.0f) * duration;
        float phase = (t / duration) * 2.0f * 3.14159f;

        BoneKeyframe kf;
        kf.time = t;
        kf.translation = skeleton.bones[i].localPosition;
        kf.rotation = skeleton.bones[i].localRotation;
        kf.scale = skeleton.bones[i].localScale;

        // Breathing: slight torso expansion
        const auto &name = track.boneName;
        if (name.find("Spine") != std::string::npos ||
            name.find("Chest") != std::string::npos) {
          float breath = std::sin(phase) * 0.02f;
          kf.scale = kf.scale + Vec3(breath, breath * 0.5f, breath);
        }

        track.keyframes.push_back(kf);
      }
      clip.tracks.push_back(track);
    }
    return clip;
  }

  // Create a basic dinosaur skeleton
  static Skeleton CreateDinosaurSkeleton() {
    Skeleton skel;
    skel.name = "DinosaurSkeleton";

    auto addBone = [&](const std::string &name, int parent, Vec3 pos) {
      Bone b;
      b.name = name;
      b.parentIndex = parent;
      b.localPosition = pos;
      b.localRotation = Quat::Identity();
      b.inverseBindMatrix.fill(0.0f);
      b.inverseBindMatrix[0] = b.inverseBindMatrix[5] =
          b.inverseBindMatrix[10] = b.inverseBindMatrix[15] = 1.0f;
      skel.bones.push_back(b);
      return static_cast<int>(skel.bones.size() - 1);
    };

    int root = addBone("Root", -1, Vec3(0, 1.5f, 0));
    int hip = addBone("Hip", root, Vec3(0, 0, 0));
    int spine1 = addBone("Spine1", hip, Vec3(0, 0.3f, 0.5f));
    int spine2 = addBone("Spine2", spine1, Vec3(0, 0.2f, 0.4f));
    int chest = addBone("Chest", spine2, Vec3(0, 0.2f, 0.3f));
    int neck = addBone("Neck", chest, Vec3(0, 0.3f, 0.3f));
    /*int head =*/addBone("Head", neck, Vec3(0, 0.2f, 0.3f));
    int tail1 = addBone("Tail1", hip, Vec3(0, 0, -0.5f));
    int tail2 = addBone("Tail2", tail1, Vec3(0, -0.1f, -0.6f));
    /*int tail3 =*/addBone("Tail3", tail2, Vec3(0, -0.1f, -0.5f));

    // Legs
    int lThigh = addBone("LeftThigh", hip, Vec3(-0.4f, -0.3f, 0));
    int lShin = addBone("LeftShin", lThigh, Vec3(0, -0.5f, 0));
    /*int lFoot =*/addBone("LeftFoot", lShin, Vec3(0, -0.5f, 0.2f));
    int rThigh = addBone("RightThigh", hip, Vec3(0.4f, -0.3f, 0));
    int rShin = addBone("RightShin", rThigh, Vec3(0, -0.5f, 0));
    /*int rFoot =*/addBone("RightFoot", rShin, Vec3(0, -0.5f, 0.2f));

    // Arms (small for T-Rex style)
    int lArm = addBone("LeftArm_L", chest, Vec3(-0.3f, -0.1f, 0.1f));
    /*int lHand =*/addBone("LeftHand_L", lArm, Vec3(0, -0.3f, 0));
    int rArm = addBone("RightArm", chest, Vec3(0.3f, -0.1f, 0.1f));
    /*int rHand =*/addBone("RightHand", rArm, Vec3(0, -0.3f, 0));

    (void)spine1;
    (void)tail1;
    (void)tail2; // suppress unused warnings

    std::cout << "[AnimationLoader] Created dinosaur skeleton: "
              << skel.bones.size() << " bones" << std::endl;
    return skel;
  }
};

} // namespace Assets
} // namespace Mesozoic
