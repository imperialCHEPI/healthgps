#pragma once
#include <benchmark/benchmark.h>
#include "../HealthGPS.Core/string_util.h"
#include <string>
#include <algorithm>

const std::string long_upper = "Practice Makes Perfect";
const std::string long_lower = "practice makes perfect";
const std::string long_greater = "practice makes younger";

const std::string short_upper = "PerFect";
const std::string short_lower = "perfect";
const std::string short_greater = "younger";

/* Equal case-insensitive functions */

std::string to_lower(std::string_view value) noexcept {
	std::string result = std::string(value);
	std::transform(value.begin(), value.end(), result.begin(),
		[](char symbol) { 
			return static_cast<unsigned char>(std::tolower(symbol));
		});

	return result;
}

bool equal_char_ascii(unsigned char left, unsigned char right) noexcept {
	return left == right || std::tolower(left) == std::tolower(right);
}

bool equal_case_insensitive_functor(const std::string_view& left, const std::string_view& right) noexcept {
	return left.size() == right.size() &&
		std::equal(left.cbegin(), left.cend(), right.cbegin(), right.cend(), equal_char_ascii);
}

bool equal_case_insensitive_lambda(const std::string_view& left, const std::string_view& right) noexcept {
	return (left.size() == right.size()) &&
		std::equal(left.cbegin(), left.cend(), right.cbegin(), right.cend(),
			[](char a, char b) noexcept { return std::tolower(a) == std::tolower(b);});
}

/* Less operation functions */

bool less_char_ascii(unsigned char left, unsigned char right) noexcept {
	return std::tolower(left) < std::tolower(right);
}

bool less_string_ascii_simple(const std::string_view& left, const std::string_view& right) {
	return to_lower(left) < to_lower(right);
}

bool less_string_ascii_functor(const std::string_view& lhs, const std::string_view& rhs) noexcept {
	return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), less_char_ascii);
}

bool less_string_ascii_lambda(const std::string_view& lhs, const std::string_view& rhs) noexcept {
	return std::lexicographical_compare(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(),
		[](char a, char b) { return std::tolower(a) < std::tolower(b); }
	);
}

/* Benchmark functions */

static void case_insensitive_equal_functor(benchmark::State& state,
	const std::string& lhs, const std::string& rhs) {
	for (auto _ : state) {
		benchmark::DoNotOptimize(equal_case_insensitive_functor(lhs, rhs));
	}
}

static void case_insensitive_equal_lambda(benchmark::State& state,
	const std::string& lhs, const std::string& rhs) {
	for (auto _ : state) {
		benchmark::DoNotOptimize(equal_case_insensitive_lambda(lhs, rhs));
	}
}

static void case_insensitive_less_simple(benchmark::State& state,
	const std::string& lhs, const std::string& rhs) {
	for (auto _ : state) {
		benchmark::DoNotOptimize(less_string_ascii_simple(lhs, rhs));
	}
}

static void case_insensitive_less_functor(benchmark::State& state,
	const std::string& lhs, const std::string& rhs) {
	for (auto _ : state) {
		benchmark::DoNotOptimize(less_string_ascii_functor(lhs, rhs));
	}
}

static void case_insensitive_less_lambda(benchmark::State& state,
	const std::string& lhs, const std::string& rhs) {
	for (auto _ : state) {
		benchmark::DoNotOptimize(less_string_ascii_lambda(lhs, rhs));
	}
}

static void case_insensitive_less_compare(benchmark::State& state,
	const std::string& lhs, const std::string& rhs) {
	using namespace hgps::core;
	auto comparer = case_insensitive::comparator{};
	for (auto _ : state) {
		benchmark::DoNotOptimize(comparer(lhs, rhs));
	}
}

BENCHMARK_CAPTURE(case_insensitive_equal_functor, str_long_yes, long_lower, long_upper);
BENCHMARK_CAPTURE(case_insensitive_equal_functor, str_long_not, long_lower, long_greater);
BENCHMARK_CAPTURE(case_insensitive_equal_functor, str_short_yes, short_lower, short_upper);
BENCHMARK_CAPTURE(case_insensitive_equal_functor, str_short_not, short_lower, short_greater);
BENCHMARK_CAPTURE(case_insensitive_equal_lambda, str_long_yes, long_lower, long_upper);
BENCHMARK_CAPTURE(case_insensitive_equal_lambda, str_long_not, long_lower, long_greater);
BENCHMARK_CAPTURE(case_insensitive_equal_lambda, str_short_yes, short_lower, short_upper);
BENCHMARK_CAPTURE(case_insensitive_equal_lambda, str_short_not, short_lower, short_greater);

BENCHMARK_CAPTURE(case_insensitive_less_simple, str_long_yes, long_lower, long_greater);
BENCHMARK_CAPTURE(case_insensitive_less_simple, str_long_not, long_greater, long_lower);
BENCHMARK_CAPTURE(case_insensitive_less_simple, str_long_none, long_lower, long_upper);
BENCHMARK_CAPTURE(case_insensitive_less_simple, str_short_yes, short_lower, short_greater);
BENCHMARK_CAPTURE(case_insensitive_less_simple, str_short_not, short_greater, short_lower);
BENCHMARK_CAPTURE(case_insensitive_less_simple, str_short_none, short_lower, long_lower);

BENCHMARK_CAPTURE(case_insensitive_less_functor, str_long_yes, long_lower, long_greater);
BENCHMARK_CAPTURE(case_insensitive_less_functor, str_long_not, long_greater, long_lower);
BENCHMARK_CAPTURE(case_insensitive_less_functor, str_long_none, long_lower, long_upper);
BENCHMARK_CAPTURE(case_insensitive_less_functor, str_short_yes, short_lower, short_greater);
BENCHMARK_CAPTURE(case_insensitive_less_functor, str_short_not, short_greater, short_lower);
BENCHMARK_CAPTURE(case_insensitive_less_functor, str_short_none, short_lower, long_lower);

BENCHMARK_CAPTURE(case_insensitive_less_lambda, str_long_yes, long_lower, long_greater);
BENCHMARK_CAPTURE(case_insensitive_less_lambda, str_long_not, long_greater, long_lower);
BENCHMARK_CAPTURE(case_insensitive_less_lambda, str_long_none, long_lower, long_upper);
BENCHMARK_CAPTURE(case_insensitive_less_lambda, str_short_yes, short_lower, short_greater);
BENCHMARK_CAPTURE(case_insensitive_less_lambda, str_short_not, short_greater, short_lower);
BENCHMARK_CAPTURE(case_insensitive_less_lambda, str_short_none, short_lower, long_lower);

BENCHMARK_CAPTURE(case_insensitive_less_compare, str_long_yes, long_lower, long_greater);
BENCHMARK_CAPTURE(case_insensitive_less_compare, str_long_not, long_greater, long_lower);
BENCHMARK_CAPTURE(case_insensitive_less_compare, str_long_none, long_lower, long_upper);
BENCHMARK_CAPTURE(case_insensitive_less_compare, str_short_yes, short_lower, short_greater);
BENCHMARK_CAPTURE(case_insensitive_less_compare, str_short_not, short_greater, short_lower);
BENCHMARK_CAPTURE(case_insensitive_less_compare, str_short_none, short_lower, long_lower);

