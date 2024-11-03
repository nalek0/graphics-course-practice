#include <array>

// Project settings:
const int W = 100;
const int H = 100;
const float X_MIN = 0.f;
const float Y_MIN = 0.f;
const float X_MAX = 1.f;
const float Y_MAX = 1.f;

struct point {
    std::array<float, 2> position;
    std::array<float, 3> color;
};
