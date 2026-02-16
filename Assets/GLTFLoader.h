#pragma once
#include "../Core/Math/Vec3.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mesozoic {
namespace Assets {

using Math::Vec3;

// =========================================================================
// glTF 2.0 Data Structures (simplified, covers common use-cases)
// =========================================================================

struct GLTFVertex {
  Vec3 position;
  Vec3 normal;
  float uv[2] = {0, 0};
  float tangent[4] = {1, 0, 0, 1};
  float boneWeights[4] = {1, 0, 0, 0};
  uint16_t boneIndices[4] = {0, 0, 0, 0};
};

struct GLTFPrimitive {
  std::vector<GLTFVertex> vertices;
  std::vector<uint32_t> indices;
  int materialIndex = -1;
};

struct GLTFMesh {
  std::string name;
  std::vector<GLTFPrimitive> primitives;
};

struct GLTFMaterial {
  std::string name;
  float baseColor[4] = {1, 1, 1, 1};
  float metallic = 0.0f;
  float roughness = 0.5f;
  int albedoTexture = -1;
  int normalTexture = -1;
  int metallicRoughnessTexture = -1;
  int occlusionTexture = -1;
};

struct GLTFTexture {
  std::string uri;
  int width = 0, height = 0;
};

struct GLTFSkin {
  std::string name;
  std::vector<int> jointIndices;
  std::vector<std::array<float, 16>> inverseBindMatrices;
};

struct GLTFNode {
  std::string name;
  int meshIndex = -1;
  int skinIndex = -1;
  float translation[3] = {0, 0, 0};
  float rotation[4] = {0, 0, 0, 1}; // quaternion xyzw
  float scale[3] = {1, 1, 1};
  std::vector<int> children;
};

struct GLTFAnimation {
  std::string name;
  struct Channel {
    int nodeIndex;
    std::string path; // "translation", "rotation", "scale"
    std::vector<float> times;
    std::vector<float> values; // 3 floats per key (trans/scale) or 4 (rotation)
  };
  std::vector<Channel> channels;
  float duration = 0.0f;
};

struct GLTFScene {
  std::vector<GLTFMesh> meshes;
  std::vector<GLTFMaterial> materials;
  std::vector<GLTFTexture> textures;
  std::vector<GLTFNode> nodes;
  std::vector<GLTFSkin> skins;
  std::vector<GLTFAnimation> animations;
  std::string name;
  bool valid = false;
};

// =========================================================================
// Minimal JSON parser for glTF (no external dependencies)
// =========================================================================

class MiniJSON {
public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  struct Value {
    Type type = Type::Null;
    double number = 0;
    bool boolean = false;
    std::string str;
    std::vector<Value> array;
    std::unordered_map<std::string, Value> object;

    const Value &operator[](const std::string &key) const {
      static Value null;
      auto it = object.find(key);
      return it != object.end() ? it->second : null;
    }

    const Value &operator[](size_t index) const {
      static Value null;
      return index < array.size() ? array[index] : null;
    }

    int AsInt() const { return static_cast<int>(number); }
    float AsFloat() const { return static_cast<float>(number); }
    bool Has(const std::string &key) const {
      return object.find(key) != object.end();
    }
    size_t Size() const { return type == Type::Array ? array.size() : 0; }
  };

  static Value Parse(const std::string &json) {
    size_t pos = 0;
    return ParseValue(json, pos);
  }

private:
  static void SkipWhitespace(const std::string &s, size_t &pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\n' ||
                              s[pos] == '\r' || s[pos] == '\t'))
      pos++;
  }

  static Value ParseValue(const std::string &s, size_t &pos) {
    SkipWhitespace(s, pos);
    if (pos >= s.size())
      return {};

    char c = s[pos];
    if (c == '"')
      return ParseString(s, pos);
    if (c == '{')
      return ParseObject(s, pos);
    if (c == '[')
      return ParseArray(s, pos);
    if (c == 't' || c == 'f')
      return ParseBool(s, pos);
    if (c == 'n') {
      pos += 4;
      return {};
    }
    return ParseNumber(s, pos);
  }

  static Value ParseString(const std::string &s, size_t &pos) {
    Value v;
    v.type = Type::String;
    pos++; // skip opening "
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\') {
        pos++;
      }
      v.str += s[pos++];
    }
    pos++; // skip closing "
    return v;
  }

  static Value ParseNumber(const std::string &s, size_t &pos) {
    Value v;
    v.type = Type::Number;
    size_t start = pos;
    if (s[pos] == '-')
      pos++;
    while (pos < s.size() &&
           (std::isdigit(s[pos]) || s[pos] == '.' || s[pos] == 'e' ||
            s[pos] == 'E' || s[pos] == '+' || s[pos] == '-'))
      pos++;
    v.number = std::stod(s.substr(start, pos - start));
    return v;
  }

  static Value ParseBool(const std::string &s, size_t &pos) {
    Value v;
    v.type = Type::Bool;
    if (s[pos] == 't') {
      v.boolean = true;
      pos += 4;
    } else {
      v.boolean = false;
      pos += 5;
    }
    return v;
  }

  static Value ParseArray(const std::string &s, size_t &pos) {
    Value v;
    v.type = Type::Array;
    pos++; // skip [
    SkipWhitespace(s, pos);
    while (pos < s.size() && s[pos] != ']') {
      v.array.push_back(ParseValue(s, pos));
      SkipWhitespace(s, pos);
      if (pos < s.size() && s[pos] == ',')
        pos++;
      SkipWhitespace(s, pos);
    }
    pos++; // skip ]
    return v;
  }

  static Value ParseObject(const std::string &s, size_t &pos) {
    Value v;
    v.type = Type::Object;
    pos++; // skip {
    SkipWhitespace(s, pos);
    while (pos < s.size() && s[pos] != '}') {
      Value key = ParseString(s, pos);
      SkipWhitespace(s, pos);
      pos++; // skip :
      v.object[key.str] = ParseValue(s, pos);
      SkipWhitespace(s, pos);
      if (pos < s.size() && s[pos] == ',')
        pos++;
      SkipWhitespace(s, pos);
    }
    pos++; // skip }
    return v;
  }
};

// =========================================================================
// glTF Loader
// =========================================================================

class GLTFLoader {
public:
  static GLTFScene Load(const std::string &filepath) {
    GLTFScene scene;
    scene.name = filepath;

    std::ifstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[GLTFLoader] Failed to open: " << filepath << std::endl;
      return scene;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();
    file.close();

    auto root = MiniJSON::Parse(json);
    if (root.type != MiniJSON::Type::Object) {
      std::cerr << "[GLTFLoader] Invalid JSON" << std::endl;
      return scene;
    }

    // Parse materials
    if (root.Has("materials")) {
      for (size_t i = 0; i < root["materials"].Size(); i++) {
        const auto &m = root["materials"][i];
        GLTFMaterial mat;
        mat.name = m["name"].str;
        if (m.Has("pbrMetallicRoughness")) {
          const auto &pbr = m["pbrMetallicRoughness"];
          if (pbr.Has("baseColorFactor")) {
            for (int j = 0;
                 j < 4 && j < static_cast<int>(pbr["baseColorFactor"].Size());
                 j++)
              mat.baseColor[j] = pbr["baseColorFactor"][j].AsFloat();
          }
          mat.metallic = pbr.Has("metallicFactor")
                             ? pbr["metallicFactor"].AsFloat()
                             : 0.0f;
          mat.roughness = pbr.Has("roughnessFactor")
                              ? pbr["roughnessFactor"].AsFloat()
                              : 0.5f;
          if (pbr.Has("baseColorTexture"))
            mat.albedoTexture = pbr["baseColorTexture"]["index"].AsInt();
          if (pbr.Has("metallicRoughnessTexture"))
            mat.metallicRoughnessTexture =
                pbr["metallicRoughnessTexture"]["index"].AsInt();
        }
        if (m.Has("normalTexture"))
          mat.normalTexture = m["normalTexture"]["index"].AsInt();
        scene.materials.push_back(mat);
      }
    }

    // Parse textures
    if (root.Has("textures")) {
      for (size_t i = 0; i < root["textures"].Size(); i++) {
        GLTFTexture tex;
        const auto &t = root["textures"][i];
        if (t.Has("source") && root.Has("images")) {
          int srcIdx = t["source"].AsInt();
          if (srcIdx < static_cast<int>(root["images"].Size()))
            tex.uri = root["images"][srcIdx]["uri"].str;
        }
        scene.textures.push_back(tex);
      }
    }

    // Parse nodes
    if (root.Has("nodes")) {
      for (size_t i = 0; i < root["nodes"].Size(); i++) {
        const auto &n = root["nodes"][i];
        GLTFNode node;
        node.name = n["name"].str;
        if (n.Has("mesh"))
          node.meshIndex = n["mesh"].AsInt();
        if (n.Has("skin"))
          node.skinIndex = n["skin"].AsInt();
        if (n.Has("translation")) {
          for (int j = 0; j < 3; j++)
            node.translation[j] = n["translation"][j].AsFloat();
        }
        if (n.Has("rotation")) {
          for (int j = 0; j < 4; j++)
            node.rotation[j] = n["rotation"][j].AsFloat();
        }
        if (n.Has("scale")) {
          for (int j = 0; j < 3; j++)
            node.scale[j] = n["scale"][j].AsFloat();
        }
        if (n.Has("children")) {
          for (size_t j = 0; j < n["children"].Size(); j++)
            node.children.push_back(n["children"][j].AsInt());
        }
        scene.nodes.push_back(node);
      }
    }

    // Parse animations
    if (root.Has("animations")) {
      for (size_t i = 0; i < root["animations"].Size(); i++) {
        const auto &a = root["animations"][i];
        GLTFAnimation anim;
        anim.name = a["name"].str;
        // Note: Full accessor/buffer parsing needed for real keyframe data
        // This parses the channel structure
        if (a.Has("channels")) {
          for (size_t j = 0; j < a["channels"].Size(); j++) {
            GLTFAnimation::Channel ch;
            ch.nodeIndex = a["channels"][j]["target"]["node"].AsInt();
            ch.path = a["channels"][j]["target"]["path"].str;
            anim.channels.push_back(ch);
          }
        }
        scene.animations.push_back(anim);
      }
    }

    scene.valid = true;
    std::cout << "[GLTFLoader] Loaded: " << filepath << std::endl;
    std::cout << "  Materials: " << scene.materials.size()
              << " | Textures: " << scene.textures.size()
              << " | Nodes: " << scene.nodes.size()
              << " | Animations: " << scene.animations.size() << std::endl;
    return scene;
  }

  // Create a procedural test mesh (cube)
  static GLTFMesh CreateTestCube(float size = 1.0f) {
    GLTFMesh mesh;
    mesh.name = "TestCube";
    GLTFPrimitive prim;

    float h = size * 0.5f;
    // 8 corners, 6 faces
    Vec3 corners[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                       {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
    Vec3 normals[6] = {{0, 0, -1}, {0, 0, 1},  {-1, 0, 0},
                       {1, 0, 0},  {0, -1, 0}, {0, 1, 0}};
    int faceIndices[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                             {1, 5, 6, 2}, {4, 5, 1, 0}, {3, 2, 6, 7}};

    for (int f = 0; f < 6; f++) {
      uint32_t base = static_cast<uint32_t>(prim.vertices.size());
      for (int v = 0; v < 4; v++) {
        GLTFVertex vert;
        vert.position = corners[faceIndices[f][v]];
        vert.normal = normals[f];
        vert.uv[0] = (v == 1 || v == 2) ? 1.0f : 0.0f;
        vert.uv[1] = (v >= 2) ? 1.0f : 0.0f;
        prim.vertices.push_back(vert);
      }
      prim.indices.push_back(base);
      prim.indices.push_back(base + 1);
      prim.indices.push_back(base + 2);
      prim.indices.push_back(base);
      prim.indices.push_back(base + 2);
      prim.indices.push_back(base + 3);
    }

    mesh.primitives.push_back(prim);
    return mesh;
  }

  // Create procedural dinosaur mesh (low-poly capsule body)
  static GLTFMesh CreateDinosaurPlaceholder(float length = 4.0f,
                                            float height = 2.0f) {
    GLTFMesh mesh;
    mesh.name = "DinosaurPlaceholder";
    GLTFPrimitive prim;

    const int segments = 12;
    const int rings = 6;

    for (int r = 0; r <= rings; r++) {
      float v = static_cast<float>(r) / static_cast<float>(rings);
      float z = -length * 0.5f + length * v;
      float bodyRadius = height * 0.5f * std::sin(v * 3.14159f);

      for (int s = 0; s <= segments; s++) {
        float u = static_cast<float>(s) / static_cast<float>(segments);
        float angle = u * 2.0f * 3.14159f;

        GLTFVertex vert;
        vert.position = Vec3(std::cos(angle) * bodyRadius,
                             std::sin(angle) * bodyRadius + height * 0.5f, z);
        vert.normal = Vec3(std::cos(angle), std::sin(angle), 0).Normalized();
        vert.uv[0] = u;
        vert.uv[1] = v;
        prim.vertices.push_back(vert);
      }
    }

    for (int r = 0; r < rings; r++) {
      for (int s = 0; s < segments; s++) {
        uint32_t a = static_cast<uint32_t>(r * (segments + 1) + s);
        uint32_t b = a + 1;
        uint32_t c = a + static_cast<uint32_t>(segments + 1);
        uint32_t d = c + 1;
        prim.indices.push_back(a);
        prim.indices.push_back(c);
        prim.indices.push_back(b);
        prim.indices.push_back(b);
        prim.indices.push_back(c);
        prim.indices.push_back(d);
      }
    }

    mesh.primitives.push_back(prim);
    std::cout << "[GLTFLoader] Created dinosaur placeholder: "
              << prim.vertices.size() << " verts, " << prim.indices.size() / 3
              << " tris" << std::endl;
    return mesh;
  }

  // Create a single optimized blade mesh with segments for bending
  static GLTFMesh CreateGrassMesh(float height = 1.0f) {
    GLTFMesh mesh;
    mesh.name = "GrassBlade";
    GLTFPrimitive prim;

    // 5 vertical segments for smooth bending
    const int segments = 5;
    float width = 0.15f; 

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments; // 0.0 to 1.0
        float h = t * height;
        float w = width * (1.0f - t*t); // Taper to point
        
        // UV y goes from 0 (bottom) to 1 (top)
        GLTFVertex vLeft;
        vLeft.position = Vec3(-w, h, 0); 
        vLeft.normal = Vec3(0, 0, 1); // Will be softened in shader
        vLeft.uv[0] = 0; vLeft.uv[1] = t; 
        prim.vertices.push_back(vLeft);

        GLTFVertex vRight;
        vRight.position = Vec3(w, h, 0); 
        vRight.normal = Vec3(0, 0, 1);
        vRight.uv[0] = 1; vRight.uv[1] = t; 
        prim.vertices.push_back(vRight);
    }

    // Indices
    for (int i = 0; i < segments; i++) {
        uint32_t b = i * 2;
        // Tri 1
        prim.indices.push_back(b);
        prim.indices.push_back(b + 1);
        prim.indices.push_back(b + 2);
        // Tri 2
        prim.indices.push_back(b + 1);
        prim.indices.push_back(b + 3);
        prim.indices.push_back(b + 2);
    }

    mesh.primitives.push_back(prim);
    return mesh;
  }
};

} // namespace Assets
} // namespace Mesozoic
