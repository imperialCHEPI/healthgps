#pragma once
#include <benchmark/benchmark.h>
#include "HealthGPS.Core/identifier.h"

#include <string>
#include <ostream>
#include <algorithm>

using namespace hgps::core;

struct IdentifierEx final
{
    constexpr IdentifierEx() = default;
    IdentifierEx(const char* const value)
        : IdentifierEx{ std::string(value) } {}

    IdentifierEx(std::string value)
        : value_{ std::move(value) } {}

    std::string to_string() const noexcept {
        return value_;
    }

    auto operator<=>(const IdentifierEx& rhs) const noexcept = default;

    friend std::ostream& operator<<(std::ostream& os, const IdentifierEx& identifier) {
        os << identifier.to_string();
        return os;
    }

private:
    std::string value_{};
};

const auto source_id = Identifier{ "energy" };
const auto target_id = Identifier{ "energy_zone" };
const auto same_id = Identifier{ "energy" };

const auto source_ex = IdentifierEx{ "energy" };
const auto target_ex = IdentifierEx{ "energy_zone" };
const auto same_ex = IdentifierEx{ "energy" };

static void identifier_equality(benchmark::State& state, const Identifier& lhs, const Identifier& rhs)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs == rhs);
    }
}

static void identifier_compare(benchmark::State& state, const Identifier& lhs, const Identifier& rhs)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs < rhs);
    }
}

static void identifier_ex_equality(benchmark::State& state, const IdentifierEx& lhs, const IdentifierEx& rhs)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs == rhs);
    }
}

static void identifier_ex_compare(benchmark::State& state, const IdentifierEx& lhs, const IdentifierEx& rhs)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(lhs < rhs);
    }
}

BENCHMARK_CAPTURE(identifier_equality, identifier_equal_target, source_id, target_id);
BENCHMARK_CAPTURE(identifier_ex_equality, identifier_ex_equal_target, source_ex, target_ex);

BENCHMARK_CAPTURE(identifier_equality, identifier_equal_same, source_id, same_id);
BENCHMARK_CAPTURE(identifier_ex_equality, identifier_ex_equal_same, source_ex, same_ex);

BENCHMARK_CAPTURE(identifier_compare, identifier_compare_target, source_id, target_id);
BENCHMARK_CAPTURE(identifier_ex_compare, identifier_ex_compare_target, source_ex, target_ex);

BENCHMARK_CAPTURE(identifier_compare, identifier_compare_same, source_id, same_id);
BENCHMARK_CAPTURE(identifier_ex_compare, identifier_ex_compare_same, source_ex, same_ex);
