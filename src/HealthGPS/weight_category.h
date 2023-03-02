#pragma once
#include <cstdint>

namespace hgps {

    /// @brief Enumerates the Person weight categories
    enum class WeightCategory : uint8_t {
        /// @brief Normal body fat weight
        normal,

        /// @brief Above a weight considered normal or desirable.
        overweight,

        /// @brief A person who has excess body fat.
        obese
    };
}