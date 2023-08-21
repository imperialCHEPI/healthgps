#include "univariate_summary.h"

#include <cmath>
#include <fmt/format.h>
#include <sstream>

namespace hgps::core {
UnivariateSummary::UnivariateSummary()
    : name_{"Untitled"}, min_{std::nan("")}, max_{std::nan("")} {}

UnivariateSummary::UnivariateSummary(const std::string name)
    : name_{name}, min_{std::nan("")}, max_{std::nan("")} {}

UnivariateSummary::UnivariateSummary(const std::vector<double> values) : UnivariateSummary() {
    append(values);
}

UnivariateSummary::UnivariateSummary(const std::string name, const std::vector<double> values)
    : UnivariateSummary(name) {
    append(values);
}

std::string UnivariateSummary::name() const noexcept { return name_; }

bool UnivariateSummary::is_empty() const noexcept { return moments_[0] < 1.0; }

unsigned int UnivariateSummary::count_valid() const noexcept {
    return static_cast<unsigned int>(moments_[0]);
}

unsigned int UnivariateSummary::count_null() const noexcept { return null_count_; }

unsigned int UnivariateSummary::count_total() const noexcept {
    return count_valid() + count_null();
}

double UnivariateSummary::min() const noexcept { return min_; }

double UnivariateSummary::max() const noexcept { return max_; }

double UnivariateSummary::range() const noexcept { return max_ - min_; }

double UnivariateSummary::sum() const noexcept { return moments_[0] * moments_[1]; }

double UnivariateSummary::average() const noexcept { return moments_[1]; }

double UnivariateSummary::variance() const noexcept {
    if (count_valid() < 2) {
        return std::nan("");
    }

    return (moments_[2] * moments_[0]) / (moments_[0] - 1.0);
}

double UnivariateSummary::std_deviation() const noexcept { return std::sqrt(variance()); }

double UnivariateSummary::std_error() const noexcept { return std::sqrt(variance() / moments_[0]); }

double UnivariateSummary::kurtosis() const noexcept {
    if (moments_[0] < 4.0) {
        return std::nan("");
    }

    double k_fact = (moments_[0] - 2.0) * (moments_[0] - 3.0);
    double n1 = moments_[0] - 1.0;
    double v = variance();
    return (moments_[4] * moments_[0] * moments_[0] * (moments_[0] + 1.0) / (v * v * n1) -
            n1 * n1 * 3.0) /
           k_fact;
}

double UnivariateSummary::skewness() const noexcept {
    if (moments_[0] < 3.0) {
        return std::nan("");
    }

    double v = variance();
    return moments_[3] * moments_[0] * moments_[0] /
           (std::sqrt(v) * v * (moments_[0] - 1.0) * (moments_[0] - 2.0));
}

void UnivariateSummary::clear() noexcept {
    min_ = std::nan("");
    max_ = std::nan("");
    null_count_ = 0;
    std::fill(std::begin(moments_), std::end(moments_), 0.0);
}

void UnivariateSummary::append(double value) noexcept {
    if (is_empty()) {
        min_ = value;
        max_ = value;
    } else {
        min_ = value < min_ ? value : min_;
        max_ = value > max_ ? value : max_;
    }

    // Moments calculation
    double n = moments_[0];
    double n1 = n + 1.0;
    double n2 = n * n;
    double delta = (moments_[1] - value) / n1;
    double d2 = delta * delta;
    double d3 = delta * d2;
    double r1 = n / n1;
    moments_[4] += 4 * delta * moments_[3] + 6 * d2 * moments_[2] + (1 + n * n2) * d2 * d2;
    moments_[4] *= r1;
    moments_[3] += 3 * delta * moments_[2] + (1 - n2) * d3;
    moments_[3] *= r1;
    moments_[2] += (1 + n) * d2;
    moments_[2] *= r1;
    moments_[1] -= delta;
    moments_[0] = n1;
}

void UnivariateSummary::append(const std::optional<double> &option) noexcept {
    if (option.has_value()) {
        append(option.value());
    } else {
        append_null();
    }
}

void UnivariateSummary::append(const std::vector<double> &values) noexcept {
    for (const auto &v : values) {
        append(v);
    }
}

void UnivariateSummary::append_null() noexcept { null_count_++; }

void UnivariateSummary::append_null(unsigned int count) noexcept { null_count_ += count; }

std::string UnivariateSummary::to_string() const noexcept {

    std::stringstream ss;
    ss << fmt::format("Name ..........= {}\n", name());
    ss << fmt::format("Count total....= {}\n", count_total());
    ss << fmt::format("Count valid....= {}\n", count_valid());
    ss << fmt::format("Count null.....= {}\n", count_null());
    ss << fmt::format("Sum ...........= {}\n", sum());
    ss << fmt::format("Minimum .......= {}\n", min());
    ss << fmt::format("Maximum .......= {}\n", max());
    ss << fmt::format("Range .........= {}\n", range());
    ss << fmt::format("Average .......= {}\n", average());
    ss << fmt::format("Variance ......= {}\n", variance());
    ss << fmt::format("Std. Deviation = {}\n", std_deviation());
    ss << fmt::format("Std. Error.....= {}\n", std_error());
    ss << fmt::format("Kurtosis ......= {}\n", kurtosis());
    ss << fmt::format("Skewness ......= {}\n", skewness());
    return ss.str();
}

std::ostream &operator<<(std::ostream &stream, const UnivariateSummary &summary) {
    stream << summary.to_string();
    return stream;
}
} // namespace hgps::core
