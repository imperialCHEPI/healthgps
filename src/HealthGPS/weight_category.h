#pragma once
#include <cstdint>

namespace hgps {

    enum class WeightCategory : uint8_t {
        normal,
        overweight,
        obese
    };
}