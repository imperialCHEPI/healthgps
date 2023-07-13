#include "math_util.h"
#include <algorithm>
#include <cmath>

namespace hgps::core {
int MathHelper::radix_{0};
double MathHelper::machine_precision_{0.0};
double MathHelper::numerical_precision_{0.0};

int MathHelper::radix() noexcept {
    if (radix_ == 0) {
        compute_radix();
    }

    return radix_;
}

double MathHelper::machine_precision() noexcept {
    if (machine_precision_ == 0.0) {
        compute_machine_precision();
    }

    return machine_precision_;
}

double MathHelper::default_numerical_precision() noexcept {
    if (numerical_precision_ == 0.0) {
        numerical_precision_ = std::sqrt(machine_precision());
    }

    return numerical_precision_;
}

bool MathHelper::equal(double left, double right) noexcept {
    return equal(left, right, default_numerical_precision());
}

bool MathHelper::equal(double left, double right, double precision) noexcept {
    double norm = std::max(std::abs(left), std::abs(right));
    return norm < precision || std::abs(left - right) < precision * norm;
}

void MathHelper::compute_radix() noexcept {
    auto a = 1.0;
    auto tmp1 = 0.0;
    auto tmp2 = 0.0;
    do {
        a += a;
        tmp1 = a + 1.0;
        tmp2 = tmp1 - a;
    } while (tmp2 - 1.0 != 0.0);

    auto b = 1.0;
    while (radix_ == 0) {
        b += b;
        tmp1 = a + b;
        radix_ = static_cast<int>(tmp1 - a);
    }
}

void MathHelper::compute_machine_precision() noexcept {
    auto real_radix = static_cast<double>(radix());
    auto inverse_radix = 1.0 / real_radix;

    machine_precision_ = 1.0;
    auto local_precision = 1.0 + machine_precision_;
    while (local_precision - 1.0 != 0.0) {
        machine_precision_ *= inverse_radix;
        local_precision = 1.0 + machine_precision_;
    }
}
} // namespace hgps::core