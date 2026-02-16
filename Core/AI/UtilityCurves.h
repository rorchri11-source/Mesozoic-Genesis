#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Mesozoic {
namespace Core {
namespace AI {

enum class CurveType { Linear, Exponential, Logarithmic, Logistic, Sine };

struct ResponseCurve {
  CurveType type;
  float slope;      // m
  float exponent;   // k
  float yIntercept; // c
  float xIntercept; // b

  // Normalize input [0.0 - 1.0] -> Output Score [0.0 - 1.0]
  float Evaluate(float input) const {
    float x = std::clamp(input, 0.0f, 1.0f);
    float y = 0.0f;

    switch (type) {
    case CurveType::Linear:
      // y = mx + c
      y = slope * x + yIntercept;
      break;
    case CurveType::Exponential:
      // y = (x^k)
      y = std::pow(x, exponent) + yIntercept;
      break;
    case CurveType::Logarithmic:
      // y = log_k(x) ... adjusted for 0-1 range usually
      // Simple convex: y = x^(1/k)
      y = std::pow(x, 1.0f / (exponent > 0 ? exponent : 1.0f));
      break;
    case CurveType::Logistic:
      // Sigmoid-like
      // y = 1 / (1 + e^(-k * (x - 0.5)))
      y = 1.0f / (1.0f + std::exp(-exponent * (x - 0.5f)));
      break;
    case CurveType::Sine:
      // y = sin(x * pi)
      y = std::sin(x * 3.14159f);
      break;
    }
    return std::clamp(y, 0.0f, 1.0f);
  }
};

} // namespace AI
} // namespace Core
} // namespace Mesozoic
