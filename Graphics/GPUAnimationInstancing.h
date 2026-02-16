#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Mesozoic {
namespace Graphics {

// GPU Animation Instancing: Store bone matrices in a 2D texture
// Each row = one animation frame
// Each column = one bone matrix (4x4 = 16 floats packed into 4 RGBA texels)
// All instances read from this texture with a time offset -> desynchronized
// animation

using Mat4 = std::array<float, 16>;

struct AnimationClip {
  std::string name;
  float duration; // seconds
  float framesPerSecond;
  std::vector<std::vector<Mat4>> frames; // frames[frameIndex][boneIndex]
};

struct InstanceData {
  uint32_t entityId;
  float timeOffset;   // Desync: each dino starts at a different phase
  uint32_t clipIndex; // Which animation clip
  std::array<float, 16> worldTransform; // Model matrix
};

class GPUAnimationSystem {
public:
  static constexpr int MAX_BONES = 64;
  static constexpr int TEXTURE_WIDTH =
      MAX_BONES * 4; // 4 texels per bone (4x4 matrix)

  // Conceptually: all clips are baked into one big texture on load
  std::vector<AnimationClip> clips;
  std::vector<InstanceData> instances;

  // Register an animation clip
  uint32_t AddClip(const AnimationClip &clip) {
    clips.push_back(clip);
    return static_cast<uint32_t>(clips.size() - 1);
  }

  // Register an entity instance for rendering
  void AddInstance(uint32_t entityId, uint32_t clipIndex, float timeOffset) {
    InstanceData inst;
    inst.entityId = entityId;
    inst.clipIndex = clipIndex;
    inst.timeOffset = timeOffset;
    inst.worldTransform = {1, 0, 0, 0, 0, 1, 0, 0,
                           0, 0, 1, 0, 0, 0, 0, 1}; // Identity
    instances.push_back(inst);
  }

  // CPU reference: resolve bone matrices for an instance at time t
  std::vector<Mat4> ResolveBones(const InstanceData &inst,
                                 float globalTime) const {
    const AnimationClip &clip = clips[inst.clipIndex];
    float localTime = std::fmod(globalTime + inst.timeOffset, clip.duration);
    float frameF = localTime * clip.framesPerSecond;
    int frame0 =
        static_cast<int>(frameF) % static_cast<int>(clip.frames.size());
    int frame1 = (frame0 + 1) % static_cast<int>(clip.frames.size());
    float blend = frameF - std::floor(frameF);

    // Linear interpolation between frames (LERP on matrices - simplified)
    std::vector<Mat4> result(clip.frames[frame0].size());
    for (size_t b = 0; b < result.size(); ++b) {
      for (int j = 0; j < 16; ++j) {
        result[b][j] = clip.frames[frame0][b][j] * (1.0f - blend) +
                       clip.frames[frame1][b][j] * blend;
      }
    }
    return result;
  }

  // In GPU path: all of this happens in a Vertex Shader reading from the bone
  // texture Vertex Shader pseudocode:
  //   float row = (time + instanceOffset) * fps;
  //   mat4 bone = texelFetch(boneTexture, ivec2(boneIndex * 4, row), 0);
  //   gl_Position = VP * worldMatrix * bone * vertexPos;
};

} // namespace Graphics
} // namespace Mesozoic
