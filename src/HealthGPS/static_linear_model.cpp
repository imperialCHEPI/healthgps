#include "static_linear_model.h"
#include "HealthGPS.Core/exception.h"
#include "HealthGPS.Input/poco.h"
#include "population.h"
#include "risk_factor_adjustable_model.h"
#include "runtime_context.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fmt/core.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <ranges>
#include <sstream>
#include <string_view>
#include <syncstream>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace { // anonymous namespace

// MAHIMA: Helper function to create shared_ptr from unique_ptr before moving
template <typename T> std::shared_ptr<T> create_shared_from_unique(std::unique_ptr<T> &ptr) {
    return ptr ? std::make_shared<T>(*ptr) : nullptr;
}

} // anonymous namespace

namespace hgps {

namespace {

core::Income income_category_from_rank_bucket(std::size_t bucket, std::size_t bucket_count) {
    if (bucket_count == 4) {
        switch (bucket) {
        case 0:
            return core::Income::low;
        case 1:
            return core::Income::lowermiddle;
        case 2:
            return core::Income::uppermiddle;
        default:
            return core::Income::high;
        }
    }

    // MAHIMA: For the current legacy path we support 3-category split as
    // low/middle/high and keep the existing enum contract unchanged.
    switch (bucket) {
    case 0:
        return core::Income::low;
    case 1:
        return core::Income::middle;
    default:
        return core::Income::high;
    }
}

bool should_print_income_summary_tables(const RuntimeContext &context) {
    if (context.scenario().type() != ScenarioType::baseline) {
        return false;
    }
    const int current_year = context.time_now();
    const int start_year = context.start_time();
    // MAHIMA: Keep summary tables only for baseline start year and first update year.
    return current_year == start_year || current_year == (start_year + 1);
}

void assign_income_categories_equal_split(Population &population,
                                          std::string_view income_categories) {
    const std::size_t bucket_count = income_categories == "4" ? 4u : 3u;
    std::vector<std::pair<double, std::size_t>> ranked_people;
    ranked_people.reserve(population.size());

    for (std::size_t i = 0; i < population.size(); ++i) {
        auto &person = population[i];
        if (!person.is_active()) {
            continue;
        }
        auto it = person.risk_factors.find("income"_id);
        if (it == person.risk_factors.end()) {
            continue;
        }
        ranked_people.emplace_back(it->second, i);
    }

    if (ranked_people.empty()) {
        return;
    }

    // MAHIMA: Assign income categories by sorted rank (not threshold cutoffs) so
    // configured buckets stay balanced (difference <= 1), even when many people tie
    // at the clamped max income value.
    std::ranges::sort(ranked_people, [](const auto &lhs, const auto &rhs) {
        if (lhs.first != rhs.first) {
            return lhs.first < rhs.first;
        }
        return lhs.second < rhs.second;
    });

    const std::size_t n = ranked_people.size();
    for (std::size_t rank = 0; rank < n; ++rank) {
        const std::size_t bucket = (rank * bucket_count) / n;
        population[ranked_people[rank].second].income =
            income_category_from_rank_bucket(bucket, bucket_count);
    }
}

void assign_income_adjustment_strata_equal_split(Population &population, std::size_t bucket_count) {
    if (bucket_count < 2u) {
        return;
    }

    std::vector<std::pair<double, std::size_t>> ranked_people;
    ranked_people.reserve(population.size());

    // MAHIMA: Reset flags first so callers can reliably test assignment status per-step.
    for (auto &person : population) {
        person.has_income_adjustment_stratum = false;
        person.income_adjustment_stratum = 0u;
    }

    for (std::size_t i = 0; i < population.size(); ++i) {
        auto &person = population[i];
        if (!person.is_active()) {
            continue;
        }
        auto it = person.risk_factors.find("income"_id);
        if (it == person.risk_factors.end()) {
            continue;
        }
        ranked_people.emplace_back(it->second, i);
    }

    if (ranked_people.empty()) {
        return;
    }

    // MAHIMA: Rank-based equal split keeps strata balanced (difference <= 1) and deterministic.
    std::ranges::sort(ranked_people, [](const auto &lhs, const auto &rhs) {
        if (lhs.first != rhs.first) {
            return lhs.first < rhs.first;
        }
        return lhs.second < rhs.second;
    });

    const std::size_t n = ranked_people.size();
    for (std::size_t rank = 0; rank < n; ++rank) {
        const std::size_t bucket = (rank * bucket_count) / n;
        auto &person = population[ranked_people[rank].second];
        person.income_adjustment_stratum = bucket;
        person.has_income_adjustment_stratum = true;
    }
}

struct IncomeBucketSummary {
    std::size_t count{0};
    double min{std::numeric_limits<double>::max()};
    double max{std::numeric_limits<double>::lowest()};
    double sum{0.0};
};

// MAHIMA: We are printing this table to help understand the actual income distributions across
// final categories after income-based factors -mean adjustment, which may differ from initial
// quartile thresholds due to people NOTE: Print only in start year and start_year +1
void print_income_adjustment_strata_table(
    const Population &population, const std::vector<IncomeStratumExpectedTableEntry> &tables,
    std::size_t bucket_count, int year, std::string_view phase) {
    if (bucket_count == 0) {
        return;
    }
    std::vector<IncomeBucketSummary> summaries(bucket_count);
    const core::Identifier income_id("income");
    for (const auto &person : population) {
        if (!person.is_active() || !person.has_income_adjustment_stratum ||
            person.income_adjustment_stratum >= bucket_count) {
            continue;
        }
        auto it = person.risk_factors.find(income_id);
        if (it == person.risk_factors.end()) {
            continue;
        }
        auto &s = summaries[person.income_adjustment_stratum];
        const double v = it->second;
        s.count++;
        s.sum += v;
        s.min = std::min(s.min, v);
        s.max = std::max(s.max, v);
    }

    std::ostringstream out;
    out << "\n[INCOME BASED FACTOR MEANS ADJUSTMENT][INCOME-STRATUM ASSIGNMENT] Year " << year
        << " phase=" << phase << '\n';
    out << "+--------+----------------------+-------+-------------+-------------+-------------+\n";
    out << "| Bucket | Stratum ID           | Count | Income Min  | Income Max  | Income Mean |\n";
    out << "+--------+----------------------+-------+-------------+-------------+-------------+\n";
    for (std::size_t k = 0; k < bucket_count; ++k) {
        const auto &s = summaries[k];
        const std::string stratum_id = k < tables.size() ? tables[k].first : "(missing)";
        out << "| " << std::setw(6) << k << " | " << std::setw(20) << std::left << stratum_id
            << std::right << " | " << std::setw(5) << s.count << " | ";
        if (s.count == 0) {
            out << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a";
        } else {
            out << std::setw(11) << fmt::format("{:.3f}", s.min) << " | " << std::setw(11)
                << fmt::format("{:.3f}", s.max) << " | " << std::setw(11)
                << fmt::format("{:.3f}", s.sum / static_cast<double>(s.count));
        }
        out << " |\n";
    }
    out << "+--------+----------------------+-------+-------------+-------------+-------------+\n";
    std::osyncstream(std::cout) << out.str();
}

void print_final_income_category_table(const Population &population, std::string_view categories,
                                       const std::vector<double> &thresholds, int year,
                                       std::string_view phase) {
    const core::Identifier income_id("income");
    auto cat_count = categories == "4" ? 4u : 3u;
    std::vector<IncomeBucketSummary> summaries(cat_count);
    bool has_observed_income = false;
    double observed_income_max = std::numeric_limits<double>::lowest();
    for (const auto &person : population) {
        if (!person.is_active()) {
            continue;
        }
        auto it = person.risk_factors.find(income_id);
        if (it == person.risk_factors.end()) {
            continue;
        }
        std::size_t idx = 0;
        switch (person.income) {
        case core::Income::low:
            idx = 0;
            break;
        case core::Income::lowermiddle:
        case core::Income::middle:
            idx = 1;
            break;
        case core::Income::uppermiddle:
            idx = 2;
            break;
        case core::Income::high:
            idx = categories == "4" ? 3u : 2u;
            break;
        default:
            continue;
        }
        auto &s = summaries[idx];
        const double v = it->second;
        has_observed_income = true;
        observed_income_max = std::max(observed_income_max, v);
        s.count++;
        s.sum += v;
        s.min = std::min(s.min, v);
        s.max = std::max(s.max, v);
    }

    std::ostringstream out;
    out << "\n[YEAR 2 UPDATE INCOME CATEGORIES] Year " << year << " phase=" << phase
        << " project_requirements.categories=" << categories << '\n';
    if (categories == "4" && thresholds.size() >= 3) {
        out << "Thresholds: Q1=" << fmt::format("{:.3f}", thresholds[0])
            << ", Q2=" << fmt::format("{:.3f}", thresholds[1])
            << ", Q3=" << fmt::format("{:.3f}", thresholds[2]) << ", Q4="
            << (has_observed_income ? fmt::format("{:.3f}", observed_income_max)
                                    : std::string("n/a"))
            << '\n';
    } else if (categories != "4" && thresholds.size() >= 2) {
        out << "Thresholds: T1=" << fmt::format("{:.3f}", thresholds[0])
            << ", T2=" << fmt::format("{:.3f}", thresholds[1]) << '\n';
    }
    out << "+----------+--------+-------------+-------------+-------------+\n";
    out << "| Category | Count  | Income Min  | Income Max  | Income Mean |\n";
    out << "+----------+--------+-------------+-------------+-------------+\n";
    const std::vector<std::string> labels =
        categories == "4" ? std::vector<std::string>{"Low", "LowerMid", "UpperMid", "High"}
                          : std::vector<std::string>{"Low", "Middle", "High"};
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        const auto &s = summaries[i];
        out << "| " << std::setw(8) << std::left << labels[i] << std::right << " | " << std::setw(6)
            << s.count << " | ";
        if (s.count == 0) {
            out << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a"
                << " | " << std::setw(11) << "n/a";
        } else {
            out << std::setw(11) << fmt::format("{:.3f}", s.min) << " | " << std::setw(11)
                << fmt::format("{:.3f}", s.max) << " | " << std::setw(11)
                << fmt::format("{:.3f}", s.sum / static_cast<double>(s.count));
        }
        out << " |\n";
    }
    out << "+----------+--------+-------------+-------------+-------------+\n";
    std::osyncstream(std::cout) << out.str();
}

void print_income_stratum_adjustment_examples_table(
    const std::vector<IncomeStratumAdjustmentExampleRow> &rows, int year, std::string_view phase) {
    if (rows.empty()) {
        return;
    }
    // MAHIMA: This is a debug table for inspecting example people across income strata, so we print
    // all configured columns for transparency and easier troubleshooting. We rely on caller to
    // limit the number of rows to a reasonable amount (e.g. one per stratum) to avoid excessive
    // output.
    std::ostringstream out;
    out << "\n[MAHIMA][INCOME-STRATUM DELTA/APPLY EXAMPLES] Year " << year << " phase=" << phase
        << '\n';
    constexpr std::size_t max_rows_to_print = 10u;
    std::vector<std::size_t> selected_indices;
    selected_indices.reserve(std::min(max_rows_to_print, rows.size()));
    if (rows.size() <= max_rows_to_print) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            selected_indices.push_back(i);
        }
    } else {
        // MAHIMA: Keep output compact by showing only 10 deterministic pseudo-random rows.
        // We hash row attributes to get a stable random-like ordering across runs, then take
        // top 10.
        std::vector<std::pair<std::size_t, std::size_t>> scored_indices;
        scored_indices.reserve(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto &r = rows[i];
            std::size_t score = static_cast<std::size_t>(r.bucket + 1u) * 2654435761u;
            score ^= static_cast<std::size_t>(r.person_id) * 2246822519u;
            score ^= static_cast<std::size_t>(r.age + 4099) * 3266489917u;
            score ^= static_cast<std::size_t>(r.factor.size()) * 668265263u;
            for (const char c : r.factor) {
                score = (score * 131u) ^
                        static_cast<std::size_t>(std::tolower(static_cast<unsigned char>(c)));
            }
            scored_indices.emplace_back(score, i);
        }
        std::ranges::sort(scored_indices, [](const auto &lhs, const auto &rhs) {
            if (lhs.first != rhs.first) {
                return lhs.first < rhs.first;
            }
            return lhs.second < rhs.second;
        });
        for (std::size_t i = 0; i < max_rows_to_print; ++i) {
            selected_indices.push_back(scored_indices[i].second);
        }
    }
    out << "Showing " << selected_indices.size() << " of " << rows.size() << " sampled rows\n";
    out << "+--------+----------------------+----------+----------------------+--------+-----+-----"
           "------"
           "--+---------------+-------------+-------------+-------------+-------------+\n";
    out << "| Bucket | StratumID            | PersonID | Factor               | Sex    | Age | "
           "Expected   "
           " | SimulatedMean | Delta       | Current     | AfterDelta  | Final       |\n";
    out << "+--------+----------------------+----------+----------------------+--------+-----+-----"
           "------"
           "--+---------------+-------------+-------------+-------------+-------------+\n";
    for (const auto idx : selected_indices) {
        const auto &r = rows[idx];
        out << "| " << std::setw(6) << r.bucket << " | " << std::setw(20) << std::left
            << r.stratum_id << std::right << " | " << std::setw(8) << r.person_id << " | "
            << std::setw(20) << std::left << r.factor << std::right << " | " << std::setw(6)
            << r.sex << " | " << std::setw(3) << r.age << " | " << std::setw(11)
            << fmt::format("{:.5f}", r.expected_value) << " | " << std::setw(13)
            << fmt::format("{:.5f}", r.simulated_mean) << " | " << std::setw(11)
            << fmt::format("{:.5f}", r.delta) << " | " << std::setw(11)
            << fmt::format("{:.5f}", r.current_value) << " | " << std::setw(11)
            << fmt::format("{:.5f}", r.after_delta_value) << " | " << std::setw(11)
            << fmt::format("{:.5f}", r.final_value) << " |\n";
    }
    out << "+--------+----------------------+----------+----------------------+--------+-----+-----"
           "------"
           "--+---------------+-------------+-------------+-------------+-------------+\n";
    std::osyncstream(std::cout) << out.str();
}

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
StaticLinearModel::StaticLinearModel(
    std::shared_ptr<RiskFactorSexAgeTable> expected,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
    const std::vector<core::Identifier> &names, const std::vector<LinearModelParams> &models,
    const std::vector<core::DoubleInterval> &ranges, const std::vector<double> &lambda,
    const std::vector<double> &stddev, const Eigen::MatrixXd &cholesky,
    const std::vector<LinearModelParams> &policy_models,
    const std::vector<core::DoubleInterval> &policy_ranges, const Eigen::MatrixXd &policy_cholesky,
    std::shared_ptr<std::vector<LinearModelParams>> trend_models,
    std::shared_ptr<std::vector<core::DoubleInterval>> trend_ranges,
    std::shared_ptr<std::vector<double>> trend_lambda, double info_speed,
    const std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>>
        &rural_prevalence,
    const std::unordered_map<core::Income, LinearModelParams> &income_models,
    double physical_activity_stddev, TrendType trend_type,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox,
    std::shared_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps,
    std::shared_ptr<std::vector<LinearModelParams>> income_trend_models,
    std::shared_ptr<std::vector<core::DoubleInterval>> income_trend_ranges,
    std::shared_ptr<std::vector<double>> income_trend_lambda,
    std::shared_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors,
    bool is_continuous_income_model, const LinearModelParams &continuous_income_model,
    std::string income_categories,
    const std::unordered_map<core::Identifier, PhysicalActivityModel> &physical_activity_models,
    const std::vector<IncomeStratumExpectedTableEntry> &income_stratum_expected_tables,
    bool income_stratum_adjustment_enabled, std::size_t adjustment_income_stratum_count,
    bool has_active_policies, const std::vector<LinearModelParams> &logistic_models)
    : RiskFactorAdjustableModel{std::move(expected),       expected_trend,
                                std::move(trend_steps),    trend_type,
                                expected_income_trend,       // Pass by value, not moved
                                income_trend_decay_factors}, // Pass by value, not moved
      // Continuous income model support (FINCH approach) - must come first
      is_continuous_income_model_{is_continuous_income_model},
      continuous_income_model_{continuous_income_model},
      income_categories_{std::move(income_categories)},
      // Regular trend member variables - these are shared_ptr, so we can move them
      expected_trend_{std::move(expected_trend)},
      expected_trend_boxcox_{std::move(expected_trend_boxcox)},
      trend_models_{std::move(trend_models)}, trend_ranges_{std::move(trend_ranges)},
      trend_lambda_{std::move(trend_lambda)},
      // Income trend member variables
      trend_type_{trend_type}, expected_income_trend_{std::move(expected_income_trend)},
      expected_income_trend_boxcox_{std::move(expected_income_trend_boxcox)},
      income_trend_steps_{std::move(income_trend_steps)},
      income_trend_models_{std::move(income_trend_models)},
      income_trend_ranges_{std::move(income_trend_ranges)},
      income_trend_lambda_{std::move(income_trend_lambda)},
      income_trend_decay_factors_{std::move(income_trend_decay_factors)},
      // Common member variables
      names_{names}, models_{models}, ranges_{ranges}, lambda_{lambda}, stddev_{stddev},
      cholesky_{cholesky}, policy_models_{policy_models}, policy_ranges_{policy_ranges},
      policy_cholesky_{policy_cholesky}, info_speed_{info_speed},
      rural_prevalence_{rural_prevalence}, income_models_{income_models},
      physical_activity_stddev_{physical_activity_stddev},
      physical_activity_models_{physical_activity_models},
      has_physical_activity_models_{!physical_activity_models.empty()},
      // MAHIMA: For income based factor means adjustment - cache loaded per-stratum expected tables
      // for later use.
      income_stratum_expected_tables_{income_stratum_expected_tables},
      income_stratum_adjustment_enabled_{income_stratum_adjustment_enabled},
      adjustment_income_stratum_count_{adjustment_income_stratum_count},
      has_active_policies_{has_active_policies}, logistic_models_{logistic_models} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }

    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }

    if (ranges_.empty()) {
        throw core::HgpsException("Risk factor ranges list is empty");
    }

    if (lambda_.empty()) {
        throw core::HgpsException("Risk factor lambda list is empty");
    }

    if (stddev_.empty()) {
        throw core::HgpsException("Risk factor standard deviation list is empty");
    }

    if (!cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }

    if (policy_models_.empty()) {
        throw core::HgpsException("Intervention policy model list is empty");
    }

    if (policy_ranges_.empty()) {
        throw core::HgpsException("Intervention policy ranges list is empty");
    }

    if (!policy_cholesky_.allFinite()) {
        throw core::HgpsException("Intervention policy Cholesky matrix contains non-finite values");
    }

    // Validate UPF trend parameters only if trend type is UPFTrend
    if (trend_type_ == TrendType::UPFTrend) {
        std::cout << "\nDEBUG: Validating UPF trend parameters...";
        if (trend_models_->empty()) {
            throw core::HgpsException("Time trend model list is empty");
        }
        if (trend_ranges_->empty()) {
            throw core::HgpsException("Time trend ranges list is empty");
        }
        if (trend_lambda_->empty()) {
            throw core::HgpsException("Time trend lambda list is empty");
        }
    } else {
        std::cout << "\nDEBUG: Skipping UPF trend validation (trend_type_ != UPFTrend)";
    }

    // Validate income trend parameters if income trend is enabled
    if (trend_type_ == TrendType::IncomeTrend) {
        std::cout << "\nDEBUG: Validating income trend parameters...";
        if (!expected_income_trend_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend is missing");
        }
        if (!expected_income_trend_boxcox_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend_boxcox is missing");
        }
        if (!income_trend_steps_) {
            throw core::HgpsException("Income trend is enabled but income_trend_steps is missing");
        }
        if (!income_trend_models_) {
            throw core::HgpsException("Income trend is enabled but income_trend_models is missing");
        }
        if (!income_trend_ranges_) {
            throw core::HgpsException("Income trend is enabled but income_trend_ranges is missing");
        }
        if (!income_trend_lambda_) {
            throw core::HgpsException("Income trend is enabled but income_trend_lambda is missing");
        }
        if (!income_trend_decay_factors_) {
            throw core::HgpsException(
                "Income trend is enabled but income_trend_decay_factors is missing");
        }
    } else {
        std::cout << "\nDEBUG: Skipping income trend validation (trend_type_ != IncomeTrend)";
    }

    // Validate income trend data consistency
    if (income_trend_models_ && income_trend_models_->empty()) {
        throw core::HgpsException("Income trend model list is empty");
    }
    if (income_trend_ranges_ && income_trend_ranges_->empty()) {
        throw core::HgpsException("Income trend ranges list is empty");
    }
    if (income_trend_lambda_ && income_trend_lambda_->empty()) {
        throw core::HgpsException("Income trend lambda list is empty");
    }

    // Validate that all risk factors have income trend parameters
    // Only validate income trend parameters if income trend is enabled
    if (trend_type_ == TrendType::IncomeTrend && expected_income_trend_) {
        for (const auto &name : names_) {
            if (!expected_income_trend_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend value is missing for risk factor: " +
                    name.to_string());
            }
            if (!expected_income_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected income trend BoxCox value is "
                                          "missing for risk factor: " +
                                          name.to_string());
            }

            if (!income_trend_steps_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend steps value is missing for risk factor: " +
                    name.to_string());
            }

            if (!income_trend_decay_factors_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend decay factor is missing for risk factor: " +
                    name.to_string());
            }
        }
    } else {
        std::cout << "\nDEBUG: Skipping risk factor income trend parameter validation (trend_type_ "
                     "!= IncomeTrend or expected_income_trend_ is null)";
    }
    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }

    // Validate UPF trend parameters for all risk factors only if trend type is UPFTrend
    if (trend_type_ == TrendType::UPFTrend) {
        for (const auto &name : names_) {
            if (!expected_trend_->contains(name)) {
                throw core::HgpsException("One or more expected trend value is missing");
            }
            if (!expected_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected trend BoxCox value is missing");
            }
        }
    }
}

RiskFactorModelType StaticLinearModel::type() const noexcept { return RiskFactorModelType::Static; }

std::string StaticLinearModel::name() const noexcept { return "Static"; }

bool StaticLinearModel::is_continuous_income_model() const noexcept {
    return is_continuous_income_model_;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void StaticLinearModel::generate_risk_factors(RuntimeContext &context) {

    // MAHIMA: Initialise everyone. Order is important here.
    // STEP 1: Age and gender initialized in demographic.cpp
    // STEP 2: Region and ethnicity initialized in demographic.cpp (if available)
    // STEP 3: Sector initialized in static_linear_model.cpp (if available)
    // STEP 4: Income initialized in static_linear_model.cpp (batch processing for continuous
    // income) STEP 5: Risk factors and physical activity initialized in static_linear_model.cpp
    // (below) STEP 6: Risk factors adjusted to expected means in static_linear_model.cpp (below)
    // STEP 7: Policies in static_linear_model.cpp (below)
    // STEP 8: Trends in static_linear_model.cpp (below) depends on whether trends are enabled
    // STEP 9: Risk factors adjusted to trended expected means in static_linear_model.cpp (below)

    // STEP 3: Initialize sector for all people (if sector info is available)
    for (auto &person : context.population()) {
        initialise_sector(person, context.random());
    }

    // STEP 4: Initialize income
    // Continuous: regression (age, gender, region, ethnicity + random) for all.
    // Category assignment happens AFTER factors-mean adjustment (see below).
    // Categorical: logits + softmax -> 3 categories; no adjustment to factors mean.
    if (is_continuous_income_model_) {
        // Phase 1 only: assign continuous income via regression for all people.
        for (auto &person : context.population()) {
            double continuous_income = calculate_continuous_income(person, context.random());
            person.risk_factors["income"_id] = continuous_income;
            person.income_continuous = continuous_income;
            person.income = core::Income::unknown; // set after post-adjustment rebucketing below
        }
        // Phase 2 (diagnostic quantiles) and Phase 3 (equal-split assignment) are done after
        // adjust_risk_factors below so categories reflect adjusted income.
    } else {
        for (auto &person : context.population()) {
            initialise_categorical_income(person, context.random());
        }
    }

    // STEP 5: Initialize risk factors and physical activity
    for (auto &person : context.population()) {
        initialise_factors(context, person, context.random());
        initialise_physical_activity(context, person, context.random());
    }

    // Set logistic factors for simulated mean calculation (exclude zeros for factors with logistic
    // models)
    std::unordered_set<core::Identifier> logistic_factors_set;
    for (size_t i = 0; i < names_.size(); i++) {
        bool has_logistic = !(logistic_models_[i].coefficients.empty());
        if (has_logistic) {
            logistic_factors_set.insert(names_[i]);
        }
    }
    set_logistic_factors(logistic_factors_set);
    static std::once_flag logistic_factors_print_once;
    std::call_once(logistic_factors_print_once, [&]() {
        std::osyncstream(std::cout) << "\nSet " << logistic_factors_set.size()
                                    << " logistic factors for simulated mean calculation\n";
    });

    // MAHIMA: Initial factors-mean adjustment only when user sets
    // risk_factors.adjust_to_factors_mean to true in project_requirements. No hardcoded true/false
    // – we use config only.
    const auto &req = context.inputs().project_requirements();
    if (req.risk_factors.adjust_to_factors_mean) {
        if (is_continuous_income_model_ && income_stratum_adjustment_enabled_ &&
            !income_stratum_expected_tables_.empty()) {
            // MAHIMA: For income based factor means adjustment - with stratum mode ON, income is
            // adjusted only against the overall expected table first. Risk factors + physical
            // activity are then adjusted in per-stratum passes below (after rank-strata
            // assignment).
            if (req.income.enabled && req.income.adjust_to_factors_mean) {
                const std::vector<core::Identifier> income_only_factors{"income"_id};
                const std::vector<core::DoubleInterval> income_only_ranges{
                    core::DoubleInterval{0.0, 1e9}};
                adjust_risk_factors(context, income_only_factors,
                                    OptionalRanges{income_only_ranges}, false);
            }
        } else {
            // Feature off path: keep legacy combined adjustment flow unchanged.
            auto [extended_factors, extended_ranges] =
                build_extended_factors_list(context, names_, ranges_);

            if (extended_factors.size() > names_.size()) {
                std::vector<std::string> added;
                const core::Identifier income_id("income");
                const core::Identifier pa_id("PhysicalActivity");
                auto in_base = [&names = names_](const core::Identifier &id) {
                    return std::ranges::find(names, id) != names.end();
                };
                if (!in_base(income_id) &&
                    std::ranges::find(extended_factors, income_id) != extended_factors.end()) {
                    added.emplace_back("income");
                }
                if (!in_base(pa_id) &&
                    std::ranges::find(extended_factors, pa_id) != extended_factors.end()) {
                    added.emplace_back("physical_activity");
                }
                std::string list_str;
                for (size_t i = 0; i < added.size(); ++i) {
                    if (i > 0) {
                        list_str += ", ";
                    }
                    list_str += added[i];
                }
                std::cout << "\nIncluding " << (list_str.empty() ? "additional factors" : list_str)
                          << " in adjustment (extended from " << names_.size() << " to "
                          << extended_factors.size() << " factors)";
            }
            adjust_risk_factors(context, extended_factors,
                                extended_ranges.empty() ? std::nullopt
                                                        : OptionalRanges{extended_ranges},
                                false); // false = initial adjustment, not trended
        }
    }

    if (is_continuous_income_model_ && income_stratum_adjustment_enabled_) {
        // MAHIMA:For income based factor means adjustment - assign N adjustment strata from current
        // continuous income rank. If factors-mean adjustment is enabled, this uses the
        // post-adjustment distribution; otherwise it uses initial continuous income values for a
        // consistent fallback.
        assign_income_adjustment_strata_equal_split(context.population(),
                                                    adjustment_income_stratum_count_);
        if (should_print_income_summary_tables(context)) {
            print_income_adjustment_strata_table(
                context.population(), income_stratum_expected_tables_,
                adjustment_income_stratum_count_, context.time_now(), "initial");
        }
    }

    if (req.risk_factors.adjust_to_factors_mean && is_continuous_income_model_ &&
        income_stratum_adjustment_enabled_ && !income_stratum_expected_tables_.empty()) {
        // MAHIMA: For income based factor means adjustment - apply per-stratum RF first, then PA,
        // using each stratum's expected table and filtering to people in that adjustment stratum.
        // Income is intentionally excluded here (already handled in overall pass above).
        const core::Identifier income_id("income");
        const core::Identifier pa_id("PhysicalActivity");

        std::vector<core::Identifier> stratum_rf_factors;
        std::vector<core::DoubleInterval> stratum_rf_ranges;
        stratum_rf_factors.reserve(names_.size());
        stratum_rf_ranges.reserve(ranges_.size());
        for (std::size_t i = 0; i < names_.size() && i < ranges_.size(); ++i) {
            if (names_[i] == income_id || names_[i] == pa_id) {
                continue;
            }
            stratum_rf_factors.push_back(names_[i]);
            stratum_rf_ranges.push_back(ranges_[i]);
        }

        bool adjust_pa =
            req.physical_activity.enabled && req.physical_activity.adjust_to_factors_mean;
        bool pa_in_expected = false;
        if (adjust_pa) {
            try {
                get_expected(context, core::Gender::male, 0, pa_id, std::nullopt, false);
                pa_in_expected = true;
            } catch (...) {
                pa_in_expected = false;
            }
        }
        const std::vector<core::Identifier> pa_only_factors{pa_id};
        const std::vector<core::DoubleInterval> pa_only_ranges =
            ranges_.empty() ? std::vector<core::DoubleInterval>{}
                            : std::vector<core::DoubleInterval>{ranges_.back()};

        const std::size_t stratum_count =
            std::min(adjustment_income_stratum_count_, income_stratum_expected_tables_.size());
        std::vector<IncomeStratumAdjustmentExampleRow> debug_rows;
        debug_rows.reserve(stratum_count * 16u);
        for (std::size_t k = 0; k < stratum_count; ++k) {
            const auto *stratum_expected = income_stratum_expected_tables_[k].second.get();
            const auto &stratum_id = income_stratum_expected_tables_[k].first;
            if (!stratum_rf_factors.empty()) {
                const auto before = debug_rows.size();
                adjust_risk_factors(context, stratum_rf_factors,
                                    stratum_rf_ranges.empty() ? std::nullopt
                                                              : OptionalRanges{stratum_rf_ranges},
                                    false, stratum_expected, k, &debug_rows);
                for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                    debug_rows[idx].stratum_id = stratum_id;
                }
            }
            if (adjust_pa && pa_in_expected) {
                const auto before = debug_rows.size();
                adjust_risk_factors(context, pa_only_factors,
                                    pa_only_ranges.empty() ? std::nullopt
                                                           : OptionalRanges{pa_only_ranges},
                                    false, stratum_expected, k, &debug_rows);
                for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                    debug_rows[idx].stratum_id = stratum_id;
                }
            }
        }
        // MAHIMA: Print expanded debug examples only for the first simulation year to limit
        // runtime/log overhead in long horizon runs.
        if (context.scenario().type() == ScenarioType::baseline &&
            context.time_now() == context.start_time()) {
            print_income_stratum_adjustment_examples_table(debug_rows, context.time_now(),
                                                           "initial");
        }
    }

    // Continuous income only: after adjustment, assign income_categories (3 or 4).
    if (is_continuous_income_model_) {
        // MAHIMA: Clamping is intentionally disabled so category assignment reflects the
        // full adjusted continuous-income spread after factors-mean adjustment.
        const core::Identifier income_id("income");

        // Diagnostic: confirm adjusted income range
        double min_inc = std::numeric_limits<double>::max();
        double max_inc = std::numeric_limits<double>::lowest();
        double sum_inc = 0.0;
        size_t n_inc = 0;
        for (const auto &person : context.population()) {
            if (!person.is_active()) {
                continue;
            }
            auto it = person.risk_factors.find("income"_id);
            if (it != person.risk_factors.end()) {
                double v = it->second;
                min_inc = std::min(min_inc, v);
                max_inc = std::max(max_inc, v);
                sum_inc += v;
                n_inc++;
            }
        }
        if (n_inc > 0) {
            std::cout << "\n[INCOME] After adjustment, income stats: min=" << min_inc
                      << " max=" << max_inc << " mean=" << (sum_inc / static_cast<double>(n_inc))
                      << " n=" << n_inc;
        }
        const std::vector<double> thresholds =
            income_categories_ == "4" ? calculate_income_quartiles(context.population())
                                      : calculate_income_tertiles(context.population());
        assign_income_categories_equal_split(context.population(), income_categories_);
        if (should_print_income_summary_tables(context)) {
            print_final_income_category_table(context.population(), income_categories_, thresholds,
                                              context.time_now(), "initial");
        }
    }

    // Initialise everyone with policies and trends.
    for (auto &person : context.population()) {
        if (has_active_policies_) {
            initialise_policies(context, person, context.random(), false);
        }

        // MAHIMA: Apply trend only when user sets project_requirements.trend.enabled true.
        // trend_type_ is set from project_requirements.trend.type in model_parser (no override).
        if (req.trend.enabled) {
            switch (trend_type_) {
            case TrendType::Null:
                break;
            case TrendType::UPFTrend:
                initialise_UPF_trends(context, person);
                break;
            case TrendType::IncomeTrend:
                initialise_income_trends(context, person);
                break;
            }
        }
    }

    // MAHIMA: Trended adjustment only when user sets trend.enabled and risk_factors.trended true.
    // We use project_requirements only – no hardcoded true/false.
    if (req.trend.enabled && req.risk_factors.trended) {
        if (is_continuous_income_model_ && income_stratum_adjustment_enabled_ &&
            !income_stratum_expected_tables_.empty()) {
            // MAHIMA: For income based factor means adjustment - trended path: same ordering as
            // initial path. 1) Overall income trended adjustment (optional by project
            // requirements), 2) refresh adjustment strata from updated continuous income, 3)
            // per-stratum RF trended adjustment, 4) per-stratum PA trended adjustment.
            if (req.income.enabled && req.income.trended) {
                const std::vector<core::Identifier> income_only_factors{"income"_id};
                const std::vector<core::DoubleInterval> income_only_ranges{
                    core::DoubleInterval{0.0, 1e9}};
                adjust_risk_factors(context, income_only_factors,
                                    OptionalRanges{income_only_ranges}, true);
            }

            assign_income_adjustment_strata_equal_split(context.population(),
                                                        adjustment_income_stratum_count_);
            if (should_print_income_summary_tables(context)) {
                print_income_adjustment_strata_table(
                    context.population(), income_stratum_expected_tables_,
                    adjustment_income_stratum_count_, context.time_now(), "trended");
            }

            const core::Identifier income_id("income");
            const core::Identifier pa_id("PhysicalActivity");
            std::vector<core::Identifier> stratum_rf_factors;
            std::vector<core::DoubleInterval> stratum_rf_ranges;
            stratum_rf_factors.reserve(names_.size());
            stratum_rf_ranges.reserve(ranges_.size());
            for (std::size_t i = 0; i < names_.size() && i < ranges_.size(); ++i) {
                if (names_[i] == income_id || names_[i] == pa_id) {
                    continue;
                }
                stratum_rf_factors.push_back(names_[i]);
                stratum_rf_ranges.push_back(ranges_[i]);
            }

            bool adjust_pa = req.physical_activity.enabled && req.physical_activity.trended;
            bool pa_in_expected = false;
            if (adjust_pa) {
                try {
                    get_expected(context, core::Gender::male, 0, pa_id, std::nullopt, false);
                    pa_in_expected = true;
                } catch (...) {
                    pa_in_expected = false;
                }
            }
            const std::vector<core::Identifier> pa_only_factors{pa_id};
            const std::vector<core::DoubleInterval> pa_only_ranges =
                ranges_.empty() ? std::vector<core::DoubleInterval>{}
                                : std::vector<core::DoubleInterval>{ranges_.back()};

            const std::size_t stratum_count =
                std::min(adjustment_income_stratum_count_, income_stratum_expected_tables_.size());
            std::vector<IncomeStratumAdjustmentExampleRow> debug_rows;
            debug_rows.reserve(stratum_count * 16u);
            for (std::size_t k = 0; k < stratum_count; ++k) {
                const auto *stratum_expected = income_stratum_expected_tables_[k].second.get();
                const auto &stratum_id = income_stratum_expected_tables_[k].first;
                if (!stratum_rf_factors.empty()) {
                    const auto before = debug_rows.size();
                    adjust_risk_factors(context, stratum_rf_factors,
                                        stratum_rf_ranges.empty()
                                            ? std::nullopt
                                            : OptionalRanges{stratum_rf_ranges},
                                        true, stratum_expected, k, &debug_rows);
                    for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                        debug_rows[idx].stratum_id = stratum_id;
                    }
                }
                if (adjust_pa && pa_in_expected) {
                    const auto before = debug_rows.size();
                    adjust_risk_factors(context, pa_only_factors,
                                        pa_only_ranges.empty() ? std::nullopt
                                                               : OptionalRanges{pa_only_ranges},
                                        true, stratum_expected, k, &debug_rows);
                    for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                        debug_rows[idx].stratum_id = stratum_id;
                    }
                }
            }
            if (context.scenario().type() == ScenarioType::baseline &&
                context.time_now() == context.start_time()) {
                print_income_stratum_adjustment_examples_table(debug_rows, context.time_now(),
                                                               "trended");
            }
        } else {
            // Trended adjustment: extended list uses project_requirements (legacy path).
            auto [trended_extended_factors, trended_extended_ranges] =
                build_extended_factors_list(context, names_, ranges_, true);
            adjust_risk_factors(context, trended_extended_factors,
                                trended_extended_ranges.empty()
                                    ? std::nullopt
                                    : OptionalRanges{trended_extended_ranges},
                                true); // true = apply trend to expected values
        }

        // MAHIMA: Continuous income only: apply equal-split category reassignment
        // rebalance configured income buckets using rank-based equal split.
        if (is_continuous_income_model_) {
            // MAHIMA: Clamping is intentionally disabled in the trended path as well,
            // to keep bucketing based on full adjusted income variation.
            assign_income_categories_equal_split(context.population(), income_categories_);
        }
    } else {
        // MAHIMA: Skipped because user set trend.enabled or risk_factors.trended false in
        // project_requirements.
        std::cout << "\nSkipping trended adjustment (project_requirements.trend.enabled or "
                     "risk_factors.trended is false)";
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void StaticLinearModel::update_risk_factors(RuntimeContext &context) {
    const auto policy_start_year = static_cast<int>(context.inputs().run().policy_start_year);
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      context.time_now() >= policy_start_year);

    // Initialise newborns and update others.
    for (auto &person : context.population()) {
        if (person.age == 0) {
            initialise_sector(person, context.random());
            initialise_income(context, person, context.random());
            initialise_factors(context, person, context.random());
            initialise_physical_activity(context, person, context.random());
        } else {
            if (person.age == 18) {
                update_sector(person, context.random());
                update_income(context, person, context.random());
                update_factors(context, person, context.random());
            } else {
                update_sector(person, context.random());
                // update_income only updates 18-year-olds, so this is a no-op for other ages
                update_income(context, person, context.random());
                update_factors(context, person, context.random());
            }
        }
    }

    // MAHIMA: Initial factors-mean adjustment only when user sets
    // risk_factors.adjust_to_factors_mean true. We use project_requirements only – no hardcoded
    // true/false.
    const auto &req = context.inputs().project_requirements();
    if (req.risk_factors.adjust_to_factors_mean) {
        if (is_continuous_income_model_ && income_stratum_adjustment_enabled_ &&
            !income_stratum_expected_tables_.empty()) {
            // MAHIMA: For income based factor means adjustment - overall income-only pass first.
            if (req.income.enabled && req.income.adjust_to_factors_mean) {
                const std::vector<core::Identifier> income_only_factors{"income"_id};
                const std::vector<core::DoubleInterval> income_only_ranges{
                    core::DoubleInterval{0.0, 1e9}};
                adjust_risk_factors(context, income_only_factors,
                                    OptionalRanges{income_only_ranges}, false);
            }
        } else {
            auto [extended_factors, extended_ranges] =
                build_extended_factors_list(context, names_, ranges_);
            adjust_risk_factors(context, extended_factors,
                                extended_ranges.empty() ? std::nullopt
                                                        : OptionalRanges{extended_ranges},
                                false); // false = initial adjustment, not trended
        }
    }

    if (is_continuous_income_model_ && income_stratum_adjustment_enabled_) {
        // MAHIMA: For income based factor means adjustment - recompute yearly adjustment strata
        // from current income rank so newborns and updated cohorts are always mapped to the
        // configured N adjustment buckets.
        assign_income_adjustment_strata_equal_split(context.population(),
                                                    adjustment_income_stratum_count_);
        if (should_print_income_summary_tables(context)) {
            print_income_adjustment_strata_table(
                context.population(), income_stratum_expected_tables_,
                adjustment_income_stratum_count_, context.time_now(), "yearly-initial");
        }
    }

    if (req.risk_factors.adjust_to_factors_mean && is_continuous_income_model_ &&
        income_stratum_adjustment_enabled_ && !income_stratum_expected_tables_.empty()) {
        // MAHIMA: Phase 2 step 3 yearly path - per-stratum RF first, then PA.
        const core::Identifier income_id("income");
        const core::Identifier pa_id("PhysicalActivity");
        std::vector<core::Identifier> stratum_rf_factors;
        std::vector<core::DoubleInterval> stratum_rf_ranges;
        stratum_rf_factors.reserve(names_.size());
        stratum_rf_ranges.reserve(ranges_.size());
        for (std::size_t i = 0; i < names_.size() && i < ranges_.size(); ++i) {
            if (names_[i] == income_id || names_[i] == pa_id) {
                continue;
            }
            stratum_rf_factors.push_back(names_[i]);
            stratum_rf_ranges.push_back(ranges_[i]);
        }

        bool adjust_pa =
            req.physical_activity.enabled && req.physical_activity.adjust_to_factors_mean;
        bool pa_in_expected = false;
        if (adjust_pa) {
            try {
                get_expected(context, core::Gender::male, 0, pa_id, std::nullopt, false);
                pa_in_expected = true;
            } catch (...) {
                pa_in_expected = false;
            }
        }
        const std::vector<core::Identifier> pa_only_factors{pa_id};
        const std::vector<core::DoubleInterval> pa_only_ranges =
            ranges_.empty() ? std::vector<core::DoubleInterval>{}
                            : std::vector<core::DoubleInterval>{ranges_.back()};

        const std::size_t stratum_count =
            std::min(adjustment_income_stratum_count_, income_stratum_expected_tables_.size());
        std::vector<IncomeStratumAdjustmentExampleRow> debug_rows;
        debug_rows.reserve(stratum_count * 16u);
        for (std::size_t k = 0; k < stratum_count; ++k) {
            const auto *stratum_expected = income_stratum_expected_tables_[k].second.get();
            const auto &stratum_id = income_stratum_expected_tables_[k].first;
            if (!stratum_rf_factors.empty()) {
                const auto before = debug_rows.size();
                adjust_risk_factors(context, stratum_rf_factors,
                                    stratum_rf_ranges.empty() ? std::nullopt
                                                              : OptionalRanges{stratum_rf_ranges},
                                    false, stratum_expected, k, &debug_rows);
                for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                    debug_rows[idx].stratum_id = stratum_id;
                }
            }
            if (adjust_pa && pa_in_expected) {
                const auto before = debug_rows.size();
                adjust_risk_factors(context, pa_only_factors,
                                    pa_only_ranges.empty() ? std::nullopt
                                                           : OptionalRanges{pa_only_ranges},
                                    false, stratum_expected, k, &debug_rows);
                for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                    debug_rows[idx].stratum_id = stratum_id;
                }
            }
        }
        if (context.scenario().type() == ScenarioType::baseline &&
            context.time_now() == context.start_time()) {
            print_income_stratum_adjustment_examples_table(debug_rows, context.time_now(),
                                                           "yearly-initial");
        }
    }

    // MAHIMA: Continuous income: assign categories with rank-based equal split.
    if (is_continuous_income_model_) {
        // MAHIMA: Clamping disabled in yearly update path; factors-mean adjustment still applies.
        // MAHIMA: Reassign using equal-population rank buckets; this
        // removes redundant quartile/tertile recalculation and avoids tie-at-max
        // edge cases where the highest category can become empty.
        const std::vector<double> thresholds =
            income_categories_ == "4" ? calculate_income_quartiles(context.population())
                                      : calculate_income_tertiles(context.population());
        assign_income_categories_equal_split(context.population(), income_categories_);
        if (should_print_income_summary_tables(context)) {
            print_final_income_category_table(context.population(), income_categories_, thresholds,
                                              context.time_now(), "yearly-initial");
        }
    }

    // Initialise newborns and update others with policies and trends.
    for (auto &person : context.population()) {
        if (!person.is_active()) {
            continue;
        }

        if (person.age == 0) {
            if (has_active_policies_) {
                initialise_policies(context, person, context.random(), intervene);
            }

            // MAHIMA: Apply trend only when user sets project_requirements.trend.enabled true.
            if (req.trend.enabled) {
                switch (trend_type_) {
                case TrendType::Null:
                    break;
                case TrendType::UPFTrend:
                    initialise_UPF_trends(context, person);
                    break;
                case TrendType::IncomeTrend:
                    initialise_income_trends(context, person);
                    break;
                }
            }
        } else {
            if (has_active_policies_) {
                update_policies(context, person, intervene);
            }

            // MAHIMA: Apply trend only when project_requirements.trend.enabled is true.
            if (req.trend.enabled) {
                switch (trend_type_) {
                case TrendType::Null:
                    break;
                case TrendType::UPFTrend:
                    update_UPF_trends(context, person);
                    break;
                case TrendType::IncomeTrend:
                    update_income_trends(context, person);
                    break;
                }
            }
        }
    }

    // MAHIMA: Trended adjustment only when user sets trend.enabled and risk_factors.trended true.
    if (req.trend.enabled && req.risk_factors.trended) {
        if (is_continuous_income_model_ && income_stratum_adjustment_enabled_ &&
            !income_stratum_expected_tables_.empty()) {
            if (req.income.enabled && req.income.trended) {
                const std::vector<core::Identifier> income_only_factors{"income"_id};
                const std::vector<core::DoubleInterval> income_only_ranges{
                    core::DoubleInterval{0.0, 1e9}};
                adjust_risk_factors(context, income_only_factors,
                                    OptionalRanges{income_only_ranges}, true);
            }
            assign_income_adjustment_strata_equal_split(context.population(),
                                                        adjustment_income_stratum_count_);
            if (should_print_income_summary_tables(context)) {
                print_income_adjustment_strata_table(
                    context.population(), income_stratum_expected_tables_,
                    adjustment_income_stratum_count_, context.time_now(), "yearly-trended");
            }

            const core::Identifier income_id("income");
            const core::Identifier pa_id("PhysicalActivity");
            std::vector<core::Identifier> stratum_rf_factors;
            std::vector<core::DoubleInterval> stratum_rf_ranges;
            stratum_rf_factors.reserve(names_.size());
            stratum_rf_ranges.reserve(ranges_.size());
            for (std::size_t i = 0; i < names_.size() && i < ranges_.size(); ++i) {
                if (names_[i] == income_id || names_[i] == pa_id) {
                    continue;
                }
                stratum_rf_factors.push_back(names_[i]);
                stratum_rf_ranges.push_back(ranges_[i]);
            }

            bool adjust_pa = req.physical_activity.enabled && req.physical_activity.trended;
            bool pa_in_expected = false;
            if (adjust_pa) {
                try {
                    get_expected(context, core::Gender::male, 0, pa_id, std::nullopt, false);
                    pa_in_expected = true;
                } catch (...) {
                    pa_in_expected = false;
                }
            }
            const std::vector<core::Identifier> pa_only_factors{pa_id};
            const std::vector<core::DoubleInterval> pa_only_ranges =
                ranges_.empty() ? std::vector<core::DoubleInterval>{}
                                : std::vector<core::DoubleInterval>{ranges_.back()};

            const std::size_t stratum_count =
                std::min(adjustment_income_stratum_count_, income_stratum_expected_tables_.size());
            std::vector<IncomeStratumAdjustmentExampleRow> debug_rows;
            debug_rows.reserve(stratum_count * 16u);
            for (std::size_t k = 0; k < stratum_count; ++k) {
                const auto *stratum_expected = income_stratum_expected_tables_[k].second.get();
                const auto &stratum_id = income_stratum_expected_tables_[k].first;
                if (!stratum_rf_factors.empty()) {
                    const auto before = debug_rows.size();
                    adjust_risk_factors(context, stratum_rf_factors,
                                        stratum_rf_ranges.empty()
                                            ? std::nullopt
                                            : OptionalRanges{stratum_rf_ranges},
                                        true, stratum_expected, k, &debug_rows);
                    for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                        debug_rows[idx].stratum_id = stratum_id;
                    }
                }
                if (adjust_pa && pa_in_expected) {
                    const auto before = debug_rows.size();
                    adjust_risk_factors(context, pa_only_factors,
                                        pa_only_ranges.empty() ? std::nullopt
                                                               : OptionalRanges{pa_only_ranges},
                                        true, stratum_expected, k, &debug_rows);
                    for (std::size_t idx = before; idx < debug_rows.size(); ++idx) {
                        debug_rows[idx].stratum_id = stratum_id;
                    }
                }
            }
            if (context.scenario().type() == ScenarioType::baseline &&
                context.time_now() == context.start_time()) {
                print_income_stratum_adjustment_examples_table(debug_rows, context.time_now(),
                                                               "yearly-trended");
            }
        } else {
            auto [trended_extended_factors, trended_extended_ranges] =
                build_extended_factors_list(context, names_, ranges_, true);

            adjust_risk_factors(context, trended_extended_factors,
                                trended_extended_ranges.empty()
                                    ? std::nullopt
                                    : OptionalRanges{trended_extended_ranges},
                                true); // true = apply trend to expected values
        }

        // MAHIMA: Continuous income: apply
        // the same equal-split category reassignment used in non-trended flow.
        if (is_continuous_income_model_) {
            // MAHIMA: Clamping disabled in trended yearly update path; factors-mean
            // adjustment remains enabled and category assignment still runs below.
            // MAHIMA: Trended path uses the same balanced rank assignment,
            // keeping behaviour identical to initial assignment and avoiding
            // repeated threshold scans.
            assign_income_categories_equal_split(context.population(), income_categories_);
            if (should_print_income_summary_tables(context)) {
                const std::vector<double> thresholds =
                    income_categories_ == "4" ? calculate_income_quartiles(context.population())
                                              : calculate_income_tertiles(context.population());
                print_final_income_category_table(context.population(), income_categories_,
                                                  thresholds, context.time_now(), "yearly-trended");
            }
        }
    }

    // Apply policies if intervening.
    if (has_active_policies_) {
        for (auto &person : context.population()) {
            if (!person.is_active()) {
                continue;
            }

            apply_policies(person, intervene);
        }
    }
}

double StaticLinearModel::inverse_box_cox(double factor, double lambda) {
    // Handle extreme lambda values
    if (std::abs(lambda) < 1e-10) {
        // For lambda near zero, use exponential
        double result = std::exp(factor);
        // Check for Inf
        if (!std::isfinite(result)) {
            return 0.0; // Return safe value for Inf
        }
        return result;
    }
    // For non-zero lambda
    double base = (lambda * factor) + 1.0;
    // Check if base is valid for power
    if (base <= 0.0) {
        return 0.0; // Return safe value for negative/zero base
    }

    // Compute power and check result
    double result = std::pow(base, 1.0 / lambda);
    // Validate the result is finite
    if (!std::isfinite(result)) {
        return 0.0; // Return safe value for NaN/Inf
    }

    // Ensure result is non-negative (though power should already guarantee this)
    return std::max(0.0, result);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {
    static int factors_count = 0;
    static bool first_call = true;
    static bool summary_printed = false;
    static std::once_flag summary_print_once;
    static std::unordered_map<core::Identifier, int> risk_factor_counts;
    static std::unordered_map<core::Identifier, int> stage1_zero_counts; // Track Stage 1 zeros
    static std::unordered_map<core::Identifier, int> stage2_counts; // Track Stage 2 (BoxCox) usage
    static std::unordered_map<core::Identifier, bool>
        has_logistic_tracked; // Track which factors have logistic
    static size_t total_population_size = 0;
    static size_t initial_generation_count = 0;
    factors_count++;

    if (first_call) {
        total_population_size = context.population().size();
        initial_generation_count = total_population_size;
        first_call = false;

        // Initialize tracking for which factors have logistic models
        for (size_t i = 0; i < names_.size(); i++) {
            bool has_logistic = !(logistic_models_[i].coefficients.empty());
            has_logistic_tracked[names_[i]] = has_logistic;
        }
    }

    // Correlated residual sampling.
    auto residuals = compute_residuals(random, cholesky_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(context, person, models_);

    // Initialise residuals and risk factors (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual = residuals[i];

        // Save residual.
        person.risk_factors[residual_name] = residual;

        // Initialise risk factor.
        double expected =
            get_expected(context, person.gender, person.age, names_[i], ranges_[i], false);

        // MAHIMA: TWO-STAGE RISK FACTOR MODELING APPROACH
        // =======================================================================
        // Previous approach: Use BoxCox transformation to calculate risk factor values
        // New approach: First use logistic regression to determine if the risk factor value should
        //              be zero
        //              If non-zero, then use BoxCox transformation to calculate the actual value
        // =======================================================================

        // MAHIMA: Check if this risk factor has logistic coefficients
        // Empty logistic model means intentionally skip Stage 1 and use boxcox-only modeling
        bool has_logistic_model = !(logistic_models_[i].coefficients.empty());

        // STAGE 1: Determine if risk factor should be zero (only if logistic model exists)
        if (has_logistic_model) {
            double zero_probability = calculate_zero_probability(person, i);

            // Sample from this probability to determine if risk factor should be zero
            // if logistic regression output = 1, risk factor value = 0
            double random_sample = random.next_double(); // Uniform random value between 0 and 1
            if (random_sample < zero_probability) {
                // Risk factor should be zero (Stage 1 result)
                person.risk_factors[names_[i]] = 0.0;
                // Track assignment count for this risk factor (only during initial generation)
                if (!summary_printed) {
                    risk_factor_counts[names_[i]]++;
                    stage1_zero_counts[names_[i]]++; // Track Stage 1 zeros
                }
                continue; // Skip Stage 2, risk factor is zero
            }
            // If we reach here, Stage 1 determined the risk factor should be non-zero, proceed to
            // Stage 2
        }
        // If no logistic model, skip Stage 1 and go directly to Stage 2

        // STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
        // (This code runs whether we have a logistic model or not)
        double factor = linear[i] + residual * stddev_[i];
        factor = expected * inverse_box_cox(factor, lambda_[i]);
        factor = ranges_[i].clamp(factor);

        // Save risk factor
        person.risk_factors[names_[i]] = factor;

        // Track assignment count for this risk factor (only during initial generation)
        if (!summary_printed) {
            risk_factor_counts[names_[i]]++;
            stage2_counts[names_[i]]++; // Track Stage 2 (BoxCox) usage
        }
    }

    // Print summary once at the end of initial generation (not during updates for newborns)
    if (!summary_printed && std::cmp_equal(factors_count, initial_generation_count)) {
        std::call_once(summary_print_once, [&]() {
            // Print 2-stage modeling summary
            std::vector<std::tuple<std::string, std::string, int, int>> modeling_rows;
            std::size_t two_stage_count = 0;
            std::size_t boxcox_only_count = 0;

            for (const auto &name : names_) {
                bool has_logistic = has_logistic_tracked[name];
                const int stage2_non_zeros = stage2_counts[name];
                if (has_logistic) {
                    const int stage1_zeros = stage1_zero_counts[name];
                    modeling_rows.emplace_back(name.to_string(), "2-stage", stage1_zeros,
                                               stage2_non_zeros);
                    two_stage_count++;
                } else {
                    // Stage 1 is intentionally skipped for BoxCox-only factors.
                    modeling_rows.emplace_back(name.to_string(), "BoxCox-only", 0,
                                               stage2_non_zeros);
                    boxcox_only_count++;
                }
            }

            std::ostringstream out;
            const int factor_width = 28;
            const int model_width = 14;
            const int count_width = 16;
            const std::string separator = "+" + std::string(factor_width + 2, '-') + "+" +
                                          std::string(model_width + 2, '-') + "+" +
                                          std::string(count_width + 2, '-') + "+" +
                                          std::string(count_width + 2, '-') + "+";
            out << "\n=== TWO-STAGE MODELING SUMMARY ===";
            out << "\n" << separator;
            out << "\n| " << std::left << std::setw(factor_width) << "Risk factor" << " | "
                << std::left << std::setw(model_width) << "Model type" << " | " << std::right
                << std::setw(count_width) << "Stage 1 zeros"
                << " | " << std::setw(count_width) << "Stage 2 non-zeros"
                << " |";
            out << "\n" << separator;
            for (const auto &[factor, model, stage1_zeros, stage2_non_zeros] : modeling_rows) {
                out << "\n| " << std::left << std::setw(factor_width) << factor << " | "
                    << std::left << std::setw(model_width) << model << " | " << std::right
                    << std::setw(count_width) << stage1_zeros << " | " << std::setw(count_width)
                    << stage2_non_zeros << " |";
            }
            out << "\n" << separator;
            out << "\n2-stage factors: " << two_stage_count
                << " | BoxCox-only factors (Stage 1 skipped): " << boxcox_only_count;
            out << "\n======================================";
            std::osyncstream(std::cout) << out.str() << '\n';

            summary_printed = true;
        });
    }
}

void StaticLinearModel::update_factors(RuntimeContext &context, Person &person,
                                       Random &random) const {

    // Correlated residual sampling.
    auto new_residuals = compute_residuals(random, cholesky_);

    // Approximate risk factors with linear models.
    auto linear = compute_linear_models(context, person, models_);

    // Update residuals and risk factors (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Get the risk factor name and expected value
        double expected =
            get_expected(context, person.gender, person.age, names_[i], ranges_[i], false);

        // Blend old and new residuals
        // Update residual.
        auto residual_name = core::Identifier{names_[i].to_string() + "_residual"};
        double residual_old = person.risk_factors.at(residual_name);
        double residual = new_residuals[i] * info_speed_;
        residual += sqrt(1.0 - info_speed_ * info_speed_) * residual_old;

        // Save residual.
        person.risk_factors.at(residual_name) = residual;

        // MAHIMA: TWO-STAGE RISK FACTOR MODELING APPROACH
        // =======================================================================
        // Use both this year's zero probability and previous outcome
        // =======================================================================

        // MAHIMA: Check if this risk factor has logistic coefficients
        // Empty logistic model means intentionally skip Stage 1 and use boxcox-only modeling
        bool has_logistic_model = !(logistic_models_[i].coefficients.empty());

        // STAGE 1: Determine if risk factor should be zero (only if logistic model exists)
        if (has_logistic_model) {
            double zero_probability = calculate_zero_probability(person, i);

            // HACK: To maintain longitudinal correlation among people, amend their "probability of
            // being a zero" according to their current zero-probability...
            // ... and either 1 if they were a zero, or 0 if they were not.
            if (person.risk_factors[names_[i]] == 0) {
                zero_probability = (zero_probability + 1.0) / 2.0;
            } else {
                zero_probability = (zero_probability + 0.0) / 2.0;
            }

            // Draw random number to determine if risk factor should be zero
            if (random.next_double() < zero_probability) {
                // Risk factor should be zero
                person.risk_factors[names_[i]] = 0.0;
                continue;
            }
        }

        // STAGE 2: Calculate non-zero risk factor value using BoxCox transformation
        // (This code runs whether we have a logistic model or not)
        double factor = linear[i] + residual * stddev_[i];
        factor = expected * inverse_box_cox(factor, lambda_[i]);
        factor = ranges_[i].clamp(factor);

        // Save risk factor
        person.risk_factors.at(names_[i]) = factor;
    }
}

// This function is for intialising UPF Trends
void StaticLinearModel::initialise_UPF_trends(RuntimeContext &context, Person &person) const {

    // Approximate trends with linear models.
    auto linear = compute_linear_models(context, person, *trend_models_);

    // Initialise and apply trends (do not exist yet).
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_trend"};
        double expected = expected_trend_boxcox_->at(names_[i]);
        double trend = expected * inverse_box_cox(linear[i], (*trend_lambda_)[i]);
        trend = (*trend_ranges_)[i].clamp(trend);

        // Save trend.
        person.risk_factors[trend_name] = trend;
    }

    // Apply trends.
    update_UPF_trends(context, person);
}

// This function is for updating UPF Trends
void StaticLinearModel::update_UPF_trends(RuntimeContext &context, Person &person) const {

    // Get elapsed time (years).
    int elapsed_time = context.time_now() - context.start_time();

    // Apply trends (should exist).
    for (size_t i = 0; i < names_.size(); i++) {

        // Load trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_trend"};
        double trend = person.risk_factors.at(trend_name);

        // Apply trend to risk factor.
        double factor = person.risk_factors.at(names_[i]);
        int t = std::min(elapsed_time, get_trend_steps(names_[i]));
        factor *= pow(trend, t);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

// This function is for intialising Income Trends
void StaticLinearModel::initialise_income_trends(RuntimeContext &context, Person &person) const {
    // Check if income trend data is available
    if (!income_trend_models_ || !expected_income_trend_boxcox_ || !income_trend_lambda_ ||
        !income_trend_ranges_) {
        // If income trend data is not available, skip initialization
        std::cout << "Income trend data is not available, skipping initialization";
        return;
    }

    // Approximate income trends with linear models.
    auto linear = compute_linear_models(context, person, *income_trend_models_);

    // Initialise and apply income trends
    for (size_t i = 0; i < names_.size(); i++) {

        // Initialise income trend.
        auto trend_name = core::Identifier{names_[i].to_string() + "_income_trend"};
        double expected = expected_income_trend_boxcox_->at(names_[i]);

        double trend = expected * inverse_box_cox(linear[i], (*income_trend_lambda_)[i]);

        trend = (*income_trend_ranges_)[i].clamp(trend);

        // Save income trend.
        person.risk_factors[trend_name] = trend;
    }

    // Apply income trends.
    update_income_trends(context, person);
}

// This function is for updating Income Trends
void StaticLinearModel::update_income_trends(RuntimeContext &context, Person &person) const {
    // If income trend data was not loaded (e.g. India/PIF without full income trend CSV),
    // initialise_income_trends returns early and never sets trend keys; avoid .at() throw.
    if (!income_trend_models_ || !expected_income_trend_boxcox_ || !income_trend_lambda_ ||
        !income_trend_ranges_) {
        return;
    }
    // Get elapsed time (years). This is the income_trend_steps.
    int elapsed_time = context.time_now() - context.start_time();

    // Apply income trends
    for (size_t i = 0; i < names_.size(); i++) {

        // Load income trend (skip if never set, e.g. after early return in initialise).
        auto trend_name = core::Identifier{names_[i].to_string() + "_income_trend"};
        if (!person.risk_factors.contains(trend_name)) {
            continue;
        }
        double trend = person.risk_factors.at(trend_name);

        // Apply income trend to risk factor.
        double factor = person.risk_factors.at(names_[i]);

        // Income trend is applied from the second year (T > T0)
        if (elapsed_time > 0) {

            // Check if income trend data is available
            if (income_trend_decay_factors_ && income_trend_steps_) {
                // Get the decay factor for this risk factor
                double decay_factor = income_trend_decay_factors_->at(names_[i]);

                // Cap the trend application at income_trend_steps
                int t = std::min(elapsed_time, income_trend_steps_->at(names_[i]));

                // Calculate income trend: trend_income_T = trend_income_T0 * e^(b*(T-T0))
                double exponent = decay_factor * t;
                double trend_income_T = trend * exp(exponent);
                factor *= trend_income_T;
            }
            // If income trend data is not available, skip income trend application
        } else {
            // Skipping income trend (elapsed_time <= 0)
        }

        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

void StaticLinearModel::initialise_policies(RuntimeContext &context, Person &person, Random &random,
                                            bool intervene) const {
    // Mahima's enhancement: Skip ALL policy initialization if policies are zero
    if (!has_active_policies_) {
        return; // Skip Cholesky decomposition, residual storage, everything!
    }

    // NOTE: we need to keep baseline and intervention scenario RNGs in sync,
    //       so we compute residuals even though they are not used in baseline.

    // Intervention policy residual components.
    auto residuals = compute_residuals(random, policy_cholesky_);

    // Save residuals (never updated in lifetime).
    for (size_t i = 0; i < names_.size(); i++) {
        auto residual_name = core::Identifier{names_[i].to_string() + "_policy_residual"};
        person.risk_factors[residual_name] = residuals[i];
    }

    // Compute policies.
    update_policies(context, person, intervene);
}

void StaticLinearModel::update_policies(RuntimeContext &context, Person &person,
                                        bool intervene) const {
    // Mahima's enhancement: Skip policy computation if all policies are zero
    if (!has_active_policies_) {
        return;
    }

    // Set zero policy if not intervening.
    if (!intervene) {
        for (const auto &name : names_) {
            auto policy_name = core::Identifier{name.to_string() + "_policy"};
            person.risk_factors[policy_name] = 0.0;
        }
        return;
    }

    // Intervention policy linear components.
    auto linear = compute_linear_models(context, person, policy_models_);

    // Compute all intervention policies.
    for (size_t i = 0; i < names_.size(); i++) {

        // Load residual component.
        auto residual_name = core::Identifier{names_[i].to_string() + "_policy_residual"};
        double residual = person.risk_factors.at(residual_name);

        // Compute policy.
        auto policy_name = core::Identifier{names_[i].to_string() + "_policy"};
        double policy = linear[i] + residual;
        policy = policy_ranges_[i].clamp(policy);

        // Save policy.
        person.risk_factors[policy_name] = policy;
    }
}

void StaticLinearModel::apply_policies(Person &person, bool intervene) const {
    // Mahima's enhancement: Skip policy application if all policies are zero
    if (!has_active_policies_) {
        return;
    }

    // No-op if not intervening.
    if (!intervene) {
        return;
    }

    // Apply all intervention policies.
    for (size_t i = 0; i < names_.size(); i++) {

        // Load policy.
        auto policy_name = core::Identifier{names_[i].to_string() + "_policy"};
        double policy = person.risk_factors.at(policy_name);

        // Apply policy to risk factor.
        double factor_old = person.risk_factors.at(names_[i]);
        double factor = factor_old * (1.0 + policy / 100.0);
        factor = ranges_[i].clamp(factor);

        // Save risk factor.
        person.risk_factors.at(names_[i]) = factor;
    }
}

std::vector<double>
StaticLinearModel::compute_linear_models(RuntimeContext &context, Person &person,
                                         const std::vector<LinearModelParams> &models) const {
    std::vector<double> linear{};
    linear.reserve(names_.size());

    // MAHIMA: Age for linear models is user-specific. If project_requirements.demographics
    // max_age_for_linear_models is set and > 0, cap age to that value for age/age2/age3 terms.
    // If not set or 0, use person.age as-is (no cap). No hardcoded cap.
    const double age_for_models = [&]() {
        const auto &req = context.inputs().project_requirements();
        const auto &max_age_opt = req.demographics.max_age_for_linear_models;
        if (max_age_opt.has_value() && *max_age_opt > 0) {
            auto person_age_d = static_cast<double>(person.age);
            auto cap_d = static_cast<double>(*max_age_opt);
            return person_age_d < cap_d ? person_age_d : cap_d;
        }
        return static_cast<double>(person.age);
    }();
    const double age_squared = age_for_models * age_for_models;
    const double age_cubed = age_squared * age_for_models;

    // Cache common age identifiers for faster comparison using case sensitive approach for Age and
    // age- Mahima
    static const core::Identifier age_id("age");
    static const core::Identifier age2_id("age2");
    static const core::Identifier age3_id("age3");
    static const core::Identifier Age_id("Age");
    static const core::Identifier Age2_id("Age2");
    static const core::Identifier Age3_id("Age3");
    static const core::Identifier stddev_id("stddev");
    static const core::Identifier log_energy_intake_id("log_energy_intake");
    static const core::Identifier energyintake_id("energyintake");

    // Approximate risk factors with linear models
    for (size_t i = 0; i < names_.size(); i++) {
        if (i >= models.size()) {
            std::cout << "\nERROR: compute_linear_models - Index " << i
                      << " is out of bounds for models vector of size " << models.size();
            continue;
        }

        auto name = names_[i];
        auto model = models[i];
        double factor = model.intercept;

        for (const auto &[coefficient_name, coefficient_value] : model.coefficients) {
            // Skip the standard deviation entry as it's not a factor
            if (coefficient_name == stddev_id) {
                continue;
            }

            // Efficiently handle age-related coefficients (use age_for_models: capped if user set
            // max)
            if (coefficient_name == age_id || coefficient_name == Age_id) {
                factor += coefficient_value * age_for_models;
            } else if (coefficient_name == age2_id || coefficient_name == Age2_id) {
                factor += coefficient_value * age_squared;
            } else if (coefficient_name == age3_id || coefficient_name == Age3_id) {
                factor += coefficient_value * age_cubed;
            } else {
                // Regular coefficient processing
                try {
                    double value = person.get_risk_factor_value(coefficient_name);
                    factor += coefficient_value * value;
                } catch (const std::exception &) {
                    // If factor is missing, try to use expected value as fallback.
                    // Policy CSV row "EnergyIntake" is mapped to log_energy_intake; factors mean
                    // has "energyintake" (raw). Use expected raw energy and apply log so we never
                    // call get_expected("log_energy_intake") which would terminate if key is
                    // missing.
                    double expected_value = 0.0;
                    if (coefficient_name == log_energy_intake_id) {
                        expected_value = get_expected(context, person.gender, person.age,
                                                      energyintake_id, std::nullopt, false);
                        if (expected_value <= 0) {
                            expected_value = 1e-10;
                        }
                        factor += coefficient_value * std::log(expected_value);
                    } else {
                        try {
                            expected_value = get_expected(context, person.gender, person.age,
                                                          coefficient_name, std::nullopt, false);
                            factor += coefficient_value * expected_value;
                        } catch (const std::exception &e) {
                            std::cout << "\n[MISSING_FACTOR] Factor missing: "
                                      << coefficient_name.to_string() << " for model "
                                      << name.to_string() << " (i=" << i << ") - " << e.what();
                            throw;
                        }
                    }
                }
            }
        }

        for (const auto &[coefficient_name, coefficient_value] : model.log_coefficients) {
            try {
                double value = person.get_risk_factor_value(coefficient_name);
                if (value <= 0) {
                    value = 1e-10; // Avoid log of zero or negative
                }
                factor += coefficient_value * log(value);
            } catch (const std::exception &) {
                // If factor is missing, try expected value. For log_energy_intake use expected
                // "energyintake" (raw) and apply log, since factors mean has energyintake not
                // log_energy_intake.
                double expected_value = 0.0;
                if (coefficient_name == log_energy_intake_id) {
                    expected_value = get_expected(context, person.gender, person.age,
                                                  energyintake_id, std::nullopt, false);
                } else {
                    try {
                        expected_value = get_expected(context, person.gender, person.age,
                                                      coefficient_name, std::nullopt, false);
                    } catch (const std::exception &e) {
                        std::cout << "\n[MISSING_FACTOR] Factor missing (log): "
                                  << coefficient_name.to_string() << " for model "
                                  << name.to_string() << " (i=" << i << ") - " << e.what();
                        throw;
                    }
                }
                if (expected_value <= 0) {
                    expected_value = 1e-10;
                }
                factor += coefficient_value * log(expected_value);
            }
        }

        linear.emplace_back(factor);
    }

    return linear;
}

// Calculate the probability of a risk factor being zero using logistic regression- Mahima
double StaticLinearModel::calculate_zero_probability(Person &person,
                                                     size_t risk_factor_index) const {
    // Get the logistic model for this risk factor
    const auto &logistic_model = logistic_models_[risk_factor_index];

    // Calculate the linear predictor using the logistic model
    double logistic_linear_term = logistic_model.intercept;

    // Add the coefficients
    for (const auto &[coef_name, coef_value] : logistic_model.coefficients) {
        logistic_linear_term += coef_value * person.get_risk_factor_value(coef_name);
    }

    // logistic function: p = 1 / (1 + exp(-linear_term))
    double probability = 1.0 / (1.0 + std::exp(-logistic_linear_term));

    // Only print if probability is outside the valid range [0,1] coz logistic regression should be
    // only within 0 and 1 This should never happen with a proper logistic function, but check for
    // numerical issues
    if (probability < 0.0 || probability > 1.0) {
        std::cout << "\nWARNING: Invalid logistic probability for "
                  << names_[risk_factor_index].to_string() << ": " << probability
                  << " (should be between 0 and 1)";
    }
    return probability;
}

std::vector<double> StaticLinearModel::compute_residuals(Random &random,
                                                         const Eigen::MatrixXd &cholesky) const {
    std::vector<double> correlated_residuals{};
    correlated_residuals.reserve(names_.size());

    // Correlated samples using Cholesky decomposition.
    Eigen::VectorXd residuals{names_.size()};
    // MAHIMA: Use Eigen NullaryExpr for random fill; this avoids iterator interoperability issues
    // seen with std::ranges on MSVC and keeps clang-tidy happy about loop conversion.
    residuals = Eigen::VectorXd::NullaryExpr(
        static_cast<Eigen::Index>(names_.size()),
        [&random]([[maybe_unused]] Eigen::Index i) { return random.next_normal(0.0, 1.0); });
    residuals = cholesky * residuals;

    // Save correlated residuals.
    for (size_t i = 0; i < names_.size(); i++) {
        correlated_residuals.emplace_back(residuals[i]);
    }

    return correlated_residuals;
}

void StaticLinearModel::initialise_sector(Person &person, Random &random) const {
    // If no rural prevalence data is available, skip sector assignment
    if (rural_prevalence_.empty()) {
        return;
    }

    // Get rural prevalence for age group and sex.
    double prevalence;
    if (person.age < 18) {
        prevalence = rural_prevalence_.at("Under18"_id).at(person.gender);
    } else {
        prevalence = rural_prevalence_.at("Over18"_id).at(person.gender);
    }

    // Sample the person's sector.
    double rand = random.next_double();
    auto sector = rand < prevalence ? core::Sector::rural : core::Sector::urban;
    person.sector = sector;
}

void StaticLinearModel::update_sector(Person &person, Random &random) const {

    // If no rural prevalence data is available, skip sector update
    if (rural_prevalence_.empty()) {
        return;
    }

    // Only update rural sector 18 year olds.
    if ((person.age != 18) || (person.sector != core::Sector::rural)) {
        return;
    }

    // Get rural prevalence for age group and sex.
    double prevalence_under18 = rural_prevalence_.at("Under18"_id).at(person.gender);
    double prevalence_over18 = rural_prevalence_.at("Over18"_id).at(person.gender);

    // Compute random rural to urban transition.
    double rand = random.next_double();
    double p_rural_to_urban = 1.0 - prevalence_over18 / prevalence_under18;
    if (rand < p_rural_to_urban) {
        person.sector = core::Sector::urban;
    }
}

void StaticLinearModel::initialise_income(RuntimeContext &context, Person &person, Random &random) {
    if (is_continuous_income_model_) {
        // FINCH approach: Calculate continuous income for a single person (e.g., newborns)
        double continuous_income = calculate_continuous_income(person, context.random());
        // Store as "income" only (removed "income_continuous" storage)
        person.risk_factors["income"_id] = continuous_income;
        person.income_continuous = continuous_income; // Keep for internal use (adjustment)

        // Category assignment happens later in update_risk_factors() after factors-mean
        // adjustment. For now set temporary category; it will be replaced post-adjustment.
        person.income = core::Income::unknown; // Temporary, will be assigned after adjustment
    } else {
        // India approach: Use direct categorical assignment
        initialise_categorical_income(person, random);
        // Note: initialise_categorical_income() already stores in risk_factors["income"]
    }
}

void StaticLinearModel::update_income(RuntimeContext &context, Person &person, Random &random) {

    // Only update 18 year olds
    if (person.age == 18) {
        initialise_income(context, person, random);
    }
}

// MAHIMA: This is the India approach for categorical income assignment
// In the static_model.json, we have a set of IncomeModels as categories (e.g., low, medium,
// high)
void StaticLinearModel::initialise_categorical_income(Person &person, Random &random) {
    // India approach: Direct categorical assignment using logits and softmax
    // Compute logits for each income category
    auto logits = std::unordered_map<core::Income, double>{};
    for (const auto &[income, params] : income_models_) {
        logits[income] = params.intercept;
        for (const auto &[factor, coefficient] : params.coefficients) {
            logits.at(income) += coefficient * person.get_risk_factor_value(factor);
        }
    }

    // Compute softmax probabilities for each income category
    auto e_logits = std::unordered_map<core::Income, double>{};
    double e_logits_sum = 0.0;
    for (const auto &[income, logit] : logits) {
        e_logits[income] = exp(logit);
        e_logits_sum += e_logits.at(income);
    }

    // Compute income category probabilities
    auto probabilities = std::unordered_map<core::Income, double>{};
    for (const auto &[income, e_logit] : e_logits) {
        probabilities[income] = e_logit / e_logits_sum;
    }

    // Assign income category using CDF
    double rand = random.next_double();
    double cumulative_prob = 0.0;
    for (const auto &[income, probability] : probabilities) {
        cumulative_prob += probability;
        if (rand < cumulative_prob) {
            person.income = income;
            // Store category number as double in risk_factors["income"] for mapping system
            // This allows income to be read via mapping regardless of assignment method
            person.risk_factors["income"_id] = static_cast<double>(person.income_to_value());
            return;
        }
    }

    throw core::HgpsException("Logic Error: failed to initialise categorical income category");
}

// MAHIMA: FINCH approach for continuous income regression.
// Continuous income is computed here and stored; category assignment is deferred to
// post-adjustment rebucketing in generate/update flows.
void StaticLinearModel::initialise_continuous_income(RuntimeContext &context, Person &person,
                                                     Random &random) {
    // FINCH approach: calculate continuous income and store it.
    // Step 1: Calculate continuous income
    double continuous_income = calculate_continuous_income(person, random);

    // Store continuous income in risk factors for future use
    person.risk_factors["income_continuous"_id] = continuous_income;
    person.risk_factors["income"_id] =
        continuous_income; // Also store as "income" for mapping lookup

    // Step 2: Legacy direct conversion (kept for compatibility with older call paths).
    person.income =
        convert_income_continuous_to_category(continuous_income, context.population(), random);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
double StaticLinearModel::calculate_continuous_income(Person &person, Random &random) {
    // Calculate continuous income using the continuous income model
    double income = continuous_income_model_.intercept;

    // Add coefficient contributions for each person attribute
    for (const auto &[factor, coefficient] : continuous_income_model_.coefficients) {
        std::string factor_name = factor.to_string();

        // Skip special coefficients that are not part of the regression
        if (factor_name == "stddev" || factor_name == "min" || factor_name == "max") {
            continue;
        }

        // Dynamic coefficient matching based on factor name patterns
        double factor_value = 0.0;

        // Age effects - handle age, age2, age3, etc. dynamically
        if (factor_name.starts_with("age")) {
            if (factor_name == "age") {
                factor_value = static_cast<double>(person.age);
            } else if (factor_name == "age2") {
                factor_value = person.age * person.age;
            } else if (factor_name == "age3") {
                factor_value = person.age * person.age * person.age;
            } else {
                // Handle age4, age5, etc. dynamically
                int power = 1;
                if (factor_name.length() > 3) {
                    try {
                        power = std::stoi(factor_name.substr(3));
                    } catch (...) {
                        std::cout << "Warning: Could not parse age power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                factor_value = std::pow(person.age, power);
            }
        }
        // Gender effects - handle gender, gender2, etc. dynamically
        else if (factor_name.starts_with("gender")) {
            if (factor_name == "gender") {
                factor_value = person.gender_to_value();
            } else if (factor_name == "gender2") {
                factor_value = person.gender == core::Gender::male ? 1.0 : 0.0;
            } else {
                // Handle gender3, gender4, etc. dynamically
                int power = 1;
                if (factor_name.length() > 6) {
                    try {
                        power = std::stoi(factor_name.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse gender power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                double base_value = person.gender == core::Gender::male ? 1.0 : 0.0;
                factor_value = std::pow(base_value, power);
            }
        }
        // Region effects - handle region2, region3, region56, etc. dynamically
        else if (factor_name.starts_with("region")) {
            if (factor_name == "region") {
                // If person.region is a number, use it directly
                try {
                    factor_value = std::stod(person.region);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value =
                        (person.region == factor_name)
                            ? 1.0
                            : 0.0; // this shows if the factor value is 1 (available )then use
                                   // else if it is 0 (not available) then move on.
                }
            } else {
                // Handle region2, region3, region56, etc. dynamically
                // std::string region_number = factor_name.substr(6); // Remove "region" prefix
                factor_value = (person.region == factor_name) ? 1.0 : 0.0;
            }
        }
        // Ethnicity effects - handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
        else if (factor_name.starts_with("ethnicity")) {
            if (factor_name == "ethnicity") {
                // If person.ethnicity is a number, use it directly
                try {
                    factor_value = std::stod(person.ethnicity);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value = (person.ethnicity == factor_name) ? 1.0 : 0.0;
                }
            } else {
                // Handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
                // std::string ethnicity_number = factor_name.substr(9); // Remove "ethnicity"
                // prefix
                factor_value = (person.ethnicity == factor_name) ? 1.0 : 0.0;
            }
        }
        // Sector effects - handle sector, sector2, etc. dynamically
        else if (factor_name.starts_with("sector")) {
            if (factor_name == "sector") {
                factor_value = person.sector_to_value();
            } else {
                // Handle sector2, sector3, etc. dynamically
                int power = 1;
                if (factor_name.length() > 6) {
                    try {
                        power = std::stoi(factor_name.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse sector power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                double base_value = person.sector_to_value();
                factor_value = std::pow(base_value, power);
            }
        }
        // Income effects - handle income, income2, etc. dynamically
        else if (factor_name.starts_with("income")) {
            if (factor_name == "income") {
                factor_value = static_cast<double>(person.income);
            } else if (factor_name == "income_continuous") {
                factor_value = person.income_continuous;
            } else {
                // Handle income2, income3, etc. dynamically
                int power = 1;
                if (factor_name.length() > 6) {
                    try {
                        power = std::stoi(factor_name.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse income power from factor name: "
                                  << factor_name << '\n';
                        continue;
                    }
                }
                auto base_value = static_cast<double>(person.income);
                factor_value = std::pow(base_value, power);
            }
        }
        // Region value effects - handle any region values dynamically
        else if (factor_name == person.region || factor_name == person.ethnicity) {
            // Check if person's region or ethnicity matches this factor name exactly
            factor_value = 1.0;
        }
        // Risk factor effects - try to get from risk factors
        else {
            try {
                factor_value = person.get_risk_factor_value(factor);
            } catch (...) {
                // Factor not found, skip it
                std::cout << "Warning: Factor " << factor_name
                          << " not found for continuous income calculation, skipping.\n";
                continue;
            }
        }

        // Add the coefficient contribution
        income += coefficient * factor_value;
    }

    // Add random noise based on standard deviation
    double stddev = 0.0;
    auto stddev_it = continuous_income_model_.coefficients.find("stddev"_id);
    if (stddev_it != continuous_income_model_.coefficients.end()) {
        stddev = stddev_it->second;
    }

    if (stddev > 0.0) {
        double noise = random.next_normal(0.0, stddev);
        // Store the noise as a residual for potential future updates
        if (person.risk_factors.contains("income_continuous_residual"_id)) {
            person.risk_factors.at("income_continuous_residual"_id) = noise;
        } else {
            person.risk_factors.emplace("income_continuous_residual"_id, noise);
        }
        income += noise;
    }

    // Apply min/max bounds if they exist
    auto min_it = continuous_income_model_.coefficients.find("min"_id);
    auto max_it = continuous_income_model_.coefficients.find("max"_id);

    if (min_it != continuous_income_model_.coefficients.end()) {
        income = std::max(income, min_it->second);
    }
    if (max_it != continuous_income_model_.coefficients.end()) {
        income = std::min(income, max_it->second);
    }

    return income;
}

std::vector<double> StaticLinearModel::calculate_income_quartiles(const Population &population) {
    // Collect all valid continuous income values from the population
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());

    for (const auto &person : population) {
        if (person.is_active()) {
            auto it = person.risk_factors.find("income"_id);
            if (it != person.risk_factors.end()) {
                sorted_incomes.push_back(it->second);
            }
        }
    }
    if (sorted_incomes.empty()) {
        throw core::HgpsException(
            "No continuous income values found in population for quartile calculation");
    }

    // Sort to find quartile thresholds
    std::ranges::sort(sorted_incomes);
    size_t n = sorted_incomes.size();
    std::vector<double> quartile_thresholds(3);

    // Calculate quartile boundaries for 25%, 50%, 75%
    // For quartiles, we use the value at which X% of the population is at or below that value
    // Q1 (25th percentile): value at which 25% of population is <= Q1
    // Q2 (50th percentile/median): value at which 50% of population is <= Q2
    // Q3 (75th percentile): value at which 75% of population is <= Q3

    // Calculate indices for quartiles using proper percentile calculation
    // For n sorted elements, the p-th percentile is at index = (n-1) * p
    // We use rounding to nearest integer for better accuracy
    auto q1_index = static_cast<size_t>(std::round((n - 1) * 0.25)); // 25th percentile
    auto q2_index = static_cast<size_t>(std::round((n - 1) * 0.50)); // 50th percentile (median)
    auto q3_index = static_cast<size_t>(std::round((n - 1) * 0.75)); // 75th percentile

    // Ensure indices are within bounds
    if (q1_index >= n) {
        q1_index = n - 1;
    }
    if (q2_index >= n) {
        q2_index = n - 1;
    }
    if (q3_index >= n) {
        q3_index = n - 1;
    }

    // Set quartile thresholds
    quartile_thresholds[0] = sorted_incomes[q1_index]; // Q1: 25th percentile
    quartile_thresholds[1] = sorted_incomes[q2_index]; // Q2: 50th percentile (median)
    quartile_thresholds[2] = sorted_incomes[q3_index]; // Q3: 75th percentile

    return quartile_thresholds;
}

std::vector<double> StaticLinearModel::calculate_income_tertiles(const Population &population) {
    // Collect all valid continuous income values from the population
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());

    for (const auto &person : population) {
        if (person.is_active()) {
            auto it = person.risk_factors.find("income"_id);
            if (it != person.risk_factors.end()) {
                sorted_incomes.push_back(it->second);
            }
        }
    }
    if (sorted_incomes.empty()) {
        throw core::HgpsException(
            "No continuous income values found in population for tertile calculation");
    }

    // Sort to find tertile thresholds
    std::ranges::sort(sorted_incomes);
    size_t n = sorted_incomes.size();
    std::vector<double> tertile_thresholds(2);

    // Calculate tertile boundaries for 33%, 67%
    // For tertiles, we use the value at which X% of the population is at or below that value
    // T1 (33rd percentile): value at which 33% of population is <= T1
    // T2 (67th percentile): value at which 67% of population is <= T2

    // Calculate indices for tertiles using proper percentile calculation
    // For n sorted elements, the p-th percentile is at index = (n-1) * p
    // We use rounding to nearest integer for better accuracy
    auto t1_index = static_cast<size_t>(std::round((n - 1) * 0.33)); // 33rd percentile
    auto t2_index = static_cast<size_t>(std::round((n - 1) * 0.67)); // 67th percentile

    // Ensure indices are within bounds
    if (t1_index >= n) {
        t1_index = n - 1;
    }
    if (t2_index >= n) {
        t2_index = n - 1;
    }

    // Set tertile thresholds
    tertile_thresholds[0] = sorted_incomes[t1_index]; // T1: 33rd percentile
    tertile_thresholds[1] = sorted_incomes[t2_index]; // T2: 67th percentile

    return tertile_thresholds;
}

core::Income StaticLinearModel::convert_income_continuous_to_category(double continuous_income,
                                                                      const Population &population,
                                                                      Random & /*random*/) const {
    if (income_categories_ == "4") {
        // 4-category system: low, lowermiddle, uppermiddle, high
        // Use quartiles: 0-25% (low), 26-50% (lowermiddle), 51-75% (uppermiddle), above 75% (high)
        // Q1 = 25th percentile, Q2 = 50th percentile, Q3 = 75th percentile
        std::vector<double> quartile_thresholds = calculate_income_quartiles(population);
        if (continuous_income <= quartile_thresholds[0]) {
            return core::Income::low; // 0-25%: income <= Q1
        }
        if (continuous_income <= quartile_thresholds[1]) {
            return core::Income::lowermiddle; // 26-50%: Q1 < income <= Q2
        }
        if (continuous_income <= quartile_thresholds[2]) {
            return core::Income::uppermiddle; // 51-75%: Q2 < income <= Q3
        }
        return core::Income::high; // above 75%: income > Q3
    }
    // 3-category system: low, middle, high
    // Use tertiles: 0-33% (low), 34-67% (middle), above 67% (high)
    std::vector<double> tertile_thresholds = calculate_income_tertiles(population);
    if (continuous_income <= tertile_thresholds[0]) {
        return core::Income::low; // 0-33%
    }
    if (continuous_income <= tertile_thresholds[1]) {
        return core::Income::middle; // 34-67%
    }
    return core::Income::high; // above 67%
}

// Optimized version: uses pre-calculated thresholds (no population scan)
// For 4 categories: thresholds contains quartiles [Q1, Q2, Q3] (25th, 50th, 75th percentiles)
// For 3 categories: thresholds contains tertiles [T1, T2] (33rd, 67th percentiles)
core::Income
StaticLinearModel::convert_income_to_category(double continuous_income,
                                              const std::vector<double> &thresholds) const {
    if (income_categories_ == "4") {
        // 4-category system: low, lowermiddle, uppermiddle, high
        // Use quartiles: 0-25% (low), 26-50% (lowermiddle), 51-75% (uppermiddle), above 75% (high)
        // Q1 = 25th percentile, Q2 = 50th percentile, Q3 = 75th percentile
        // thresholds should contain [Q1, Q2, Q3]
        if (thresholds.size() < 3) {
            throw core::HgpsException("Invalid quartile thresholds for 4-category income system. "
                                      "Expected 3 thresholds, got " +
                                      std::to_string(thresholds.size()));
        }
        if (continuous_income <= thresholds[0]) {
            return core::Income::low; // 0-25%: income <= Q1
        }
        if (continuous_income <= thresholds[1]) {
            return core::Income::lowermiddle; // 26-50%: Q1 < income <= Q2
        }
        if (continuous_income <= thresholds[2]) {
            return core::Income::uppermiddle; // 51-75%: Q2 < income <= Q3
        }
        return core::Income::high; // above 75%: income > Q3
    }
    // 3-category system: low, middle, high
    // Use tertiles: 0-33% (low), 34-67% (middle), above 67% (high)
    // thresholds should contain [T1, T2]
    if (thresholds.size() < 2) {
        throw core::HgpsException("Invalid tertile thresholds for 3-category income system. "
                                  "Expected 2 thresholds, got " +
                                  std::to_string(thresholds.size()));
    }
    if (continuous_income <= thresholds[0]) {
        return core::Income::low; // 0-33%
    }
    if (continuous_income <= thresholds[1]) {
        return core::Income::middle; // 34-67%
    }
    return core::Income::high; // above 67%
}

void StaticLinearModel::initialise_physical_activity(RuntimeContext &context, Person &person,
                                                     Random &random) const {
    const auto &pa_req = context.inputs().project_requirements().physical_activity;
    if (!pa_req.enabled) {
        return;
    }
    // Use project_requirements.physical_activity.type to choose simple vs continuous (no inference)
    if (pa_req.type == "simple") {
        auto it = physical_activity_models_.find(core::Identifier("simple"));
        if (it != physical_activity_models_.end()) {
            initialise_simple_physical_activity(context, person, random, it->second);
        } else {
            PhysicalActivityModel simple_model;
            simple_model.model_type = "simple";
            simple_model.stddev = physical_activity_stddev_;
            initialise_simple_physical_activity(context, person, random, simple_model);
        }
    } else {
        auto it = physical_activity_models_.find(core::Identifier("continuous"));
        if (it == physical_activity_models_.end()) {
            throw core::HgpsException{
                "project_requirements.physical_activity.type is \"continuous\" but no continuous "
                "physical activity model is loaded. Add PhysicalActivityModels.continuous in "
                "static_model.json or set physical_activity.type to \"simple\" in config.json."};
        }
        initialise_continuous_physical_activity(context, person, random, it->second);
    }
}

// MAHIMA: Function to initialise continuous physical activity model using regression.
// This is for Finch or any project that has region, ethnicity, income continuous etc in the CSV
// file.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void StaticLinearModel::initialise_continuous_physical_activity(
    [[maybe_unused]] RuntimeContext &context, Person &person, Random &random,
    const PhysicalActivityModel &model) {
    // Start with the intercept
    double value = model.intercept;

    // Process coefficients dynamically from CSV file
    for (const auto &[factor_name, coefficient] : model.coefficients) {
        std::string factor_name_str = factor_name.to_string();

        // Skip special coefficients that are not part of the regression
        if (factor_name_str == "stddev" || factor_name_str == "min" || factor_name_str == "max") {
            continue;
        }

        // Dynamic coefficient matching based on factor name patterns
        double factor_value = 0.0;

        // Age effects - handle age, age2, age3, etc. dynamically
        if (factor_name_str.starts_with("age")) {
            if (factor_name_str == "age") {
                factor_value = static_cast<double>(person.age);
            } else if (factor_name_str == "age2") {
                factor_value = person.age * person.age;
            } else if (factor_name_str == "age3") {
                factor_value = person.age * person.age * person.age;
            } else {
                // Handle age4, age5, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 3) {
                    try {
                        power = std::stoi(factor_name_str.substr(3));
                    } catch (...) {
                        std::cout << "Warning: Could not parse age power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                factor_value = std::pow(person.age, power);
            }
        }
        // Gender effects - handle gender, gender2, etc. dynamically
        else if (factor_name_str.starts_with("gender")) {
            if (factor_name_str == "gender") {
                factor_value = person.gender_to_value();
            } else if (factor_name_str == "gender2") {
                factor_value = person.gender == core::Gender::male ? 1.0 : 0.0;
            } else {
                // Handle gender3, gender4, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 6) {
                    try {
                        power = std::stoi(factor_name_str.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse gender power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                double base_value = person.gender == core::Gender::male ? 1.0 : 0.0;
                factor_value = std::pow(base_value, power);
            }
        }
        // Region effects - handle region2, region3, region56, etc. dynamically
        else if (factor_name_str.starts_with("region")) {
            if (factor_name_str == "region") {
                // If person.region is a number, use it directly
                try {
                    factor_value = std::stod(person.region);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value = (person.region == factor_name_str) ? 1.0 : 0.0;
                }
            } else {
                // Handle region2, region3,.... region56, etc. dynamically
                factor_value = (person.region == factor_name_str) ? 1.0 : 0.0;
            }
        }
        // Ethnicity effects - handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
        else if (factor_name_str.starts_with("ethnicity")) {
            if (factor_name_str == "ethnicity") {
                // If person.ethnicity is a number, use it directly
                try {
                    factor_value = std::stod(person.ethnicity);
                } catch (...) {
                    // If not a number, check if it matches the factor name
                    factor_value = (person.ethnicity == factor_name_str) ? 1.0 : 0.0;
                }
            } else {
                // Handle ethnicity2, ethnicity3, ethnicity90, etc. dynamically
                // std::string ethnicity_number =
                //     factor_name_str.substr(9); // Remove "ethnicity" prefix
                factor_value = (person.ethnicity == factor_name_str) ? 1.0 : 0.0;
            }
        }
        // Sector effects - handle sector, sector2, etc. dynamically
        else if (factor_name_str.starts_with("sector")) {
            if (factor_name_str == "sector") {
                factor_value = person.sector_to_value();
            } else {
                // Handle sector2, sector3, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 6) {
                    try {
                        power = std::stoi(factor_name_str.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse sector power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                double base_value = person.sector_to_value();
                factor_value = std::pow(base_value, power);
            }
        }
        // Income effects - handle income, income2, etc. dynamically
        else if (factor_name_str.starts_with("income")) {
            if (factor_name_str == "income") {
                factor_value = static_cast<double>(person.income);
            } else if (factor_name_str == "income_continuous") {
                factor_value = person.income_continuous;
            } else {
                // Handle income2, income3, etc. dynamically
                int power = 1;
                if (factor_name_str.length() > 6) {
                    try {
                        power = std::stoi(factor_name_str.substr(6));
                    } catch (...) {
                        std::cout << "Warning: Could not parse income power from factor name: "
                                  << factor_name_str << '\n';
                        continue;
                    }
                }
                auto base_value = static_cast<double>(person.income);
                factor_value = std::pow(base_value, power);
            }
        }
        // Region value effects - handle any region values dynamically
        else if (factor_name_str == person.region || factor_name_str == person.ethnicity) {
            // Check if person's region or ethnicity matches this factor name exactly
            factor_value = 1.0;
        }
        // Risk factor effects - try to get from risk factors
        else {
            try {
                factor_value = person.get_risk_factor_value(factor_name);
            } catch (...) {
                // Factor not found, skip it
                std::cout << "Warning: Factor " << factor_name_str
                          << " not found for continuous physical activity calculation, skipping.\n";
                continue;
            }
        }

        // Add the coefficient contribution
        value += coefficient * factor_value;
    }

    // Add random noise using the model's standard deviation
    double rand_noise = random.next_normal(0.0, model.stddev);
    double final_value = value + rand_noise;

    // Apply min/max constraints
    final_value = std::max(model.min_value, std::min(final_value, model.max_value));

    // Set the physical activity value (store in both member variable and risk_factors for
    // compatibility)
    person.physical_activity = final_value;
    person.risk_factors["PhysicalActivity"_id] = final_value;
}

// MAHIMA: Function to initialise simple physical activity model using log-normal distribution
// This is for India or any project that does not have region, ethncity, income continuous etc
// in the CSV
void StaticLinearModel::initialise_simple_physical_activity(
    RuntimeContext &context, Person &person, Random &random,
    const PhysicalActivityModel &model) const {
    // India approach: Simple model with standard deviation from the model
    double expected = get_expected(context, person.gender, person.age, "PhysicalActivity"_id,
                                   std::nullopt, false);
    double rand = random.next_normal(0.0, model.stddev);
    double factor = expected * exp(rand - (0.5 * pow(model.stddev, 2)));

    // Store in both physical_activity and risk_factors for compatibility
    person.physical_activity = factor;
    person.risk_factors["PhysicalActivity"_id] = factor;
}

// MAHIMA: Build extended factors list from project_requirements only.
// For initial adjustment: adds income/PA only if
// project_requirements.income/pa.adjust_to_factors_mean is true. For trended adjustment: adds
// income/PA only if project_requirements.income/pa.trended is true. No hardcoded true/false – we
// use config only so the user controls behaviour explicitly.
std::pair<std::vector<core::Identifier>, std::vector<core::DoubleInterval>>
StaticLinearModel::build_extended_factors_list(RuntimeContext &context,
                                               const std::vector<core::Identifier> &base_factors,
                                               const std::vector<core::DoubleInterval> &base_ranges,
                                               bool for_trended_adjustment) const {
    const auto &req = context.inputs().project_requirements();
    std::vector<core::Identifier> extended_factors = base_factors;
    std::vector<core::DoubleInterval> extended_ranges = base_ranges;

    const core::Identifier income_id("income");
    const core::Identifier PhysicalActivity_id("PhysicalActivity");
    bool income_in_base = std::ranges::find(base_factors, income_id) != base_factors.end();
    bool pa_in_base = std::ranges::find(base_factors, PhysicalActivity_id) != base_factors.end();

    bool add_income =
        for_trended_adjustment ? req.income.trended : req.income.adjust_to_factors_mean;
    if (!income_in_base && req.income.enabled && add_income) {
        try {
            get_expected(context, core::Gender::male, 0, income_id, std::nullopt, false);
            extended_factors.push_back(income_id);
            if (!base_ranges.empty()) {
                extended_ranges.emplace_back(0.0, 1e9); // no effective clamp for continuous income
            }
        } catch (...) {
            // Income not in expected table - skip
        }
    }

    bool add_pa = for_trended_adjustment ? req.physical_activity.trended
                                         : req.physical_activity.adjust_to_factors_mean;
    if (!pa_in_base && req.physical_activity.enabled && add_pa) {
        try {
            get_expected(context, core::Gender::male, 0, PhysicalActivity_id, std::nullopt, false);
            extended_factors.push_back(PhysicalActivity_id);
            if (!base_ranges.empty()) {
                extended_ranges.push_back(base_ranges.back());
            }
        } catch (...) {
            // Physical activity not in expected table - skip
        }
    }

    return std::make_pair(std::move(extended_factors), std::move(extended_ranges));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
StaticLinearModelDefinition::StaticLinearModelDefinition(
    std::unique_ptr<RiskFactorSexAgeTable> expected,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> trend_steps,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_trend_boxcox,
    std::vector<core::Identifier> names, std::vector<LinearModelParams> models,
    std::vector<core::DoubleInterval> ranges, std::vector<double> lambda,
    std::vector<double> stddev, Eigen::MatrixXd cholesky,
    std::vector<LinearModelParams> policy_models, std::vector<core::DoubleInterval> policy_ranges,
    Eigen::MatrixXd policy_cholesky, std::unique_ptr<std::vector<LinearModelParams>> trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> trend_ranges,
    std::unique_ptr<std::vector<double>> trend_lambda, double info_speed,
    std::unordered_map<core::Identifier, std::unordered_map<core::Gender, double>> rural_prevalence,
    std::unordered_map<core::Income, LinearModelParams> income_models,
    double physical_activity_stddev, TrendType trend_type,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> expected_income_trend_boxcox,
    std::unique_ptr<std::unordered_map<core::Identifier, int>> income_trend_steps,
    std::unique_ptr<std::vector<LinearModelParams>> income_trend_models,
    std::unique_ptr<std::vector<core::DoubleInterval>> income_trend_ranges,
    std::unique_ptr<std::vector<double>> income_trend_lambda,
    std::unique_ptr<std::unordered_map<core::Identifier, double>> income_trend_decay_factors,
    bool is_continuous_income_model, const LinearModelParams &continuous_income_model,
    std::string income_categories,
    const std::unordered_map<core::Identifier, PhysicalActivityModel> &physical_activity_models,
    const std::vector<IncomeStratumExpectedTableEntry> &income_stratum_expected_tables,
    bool income_stratum_adjustment_enabled, std::size_t adjustment_income_stratum_count,
    bool has_active_policies, std::vector<LinearModelParams> logistic_models)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    : RiskFactorAdjustableModelDefinition{std::move(expected),
                                          expected_trend
                                              ? std::make_unique<
                                                    std::unordered_map<core::Identifier, double>>(
                                                    *expected_trend)
                                              : nullptr,
                                          trend_steps
                                              ? std::make_unique<
                                                    std::unordered_map<core::Identifier, int>>(
                                                    *trend_steps)
                                              : nullptr,
                                          trend_type},
      expected_trend_{
          expected_trend
              ? std::make_shared<std::unordered_map<core::Identifier, double>>(*expected_trend)
              : nullptr},
      expected_trend_boxcox_{expected_trend_boxcox
                                 ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                                       *expected_trend_boxcox)
                                 : nullptr},
      trend_models_{create_shared_from_unique(trend_models)},
      trend_ranges_{create_shared_from_unique(trend_ranges)},
      trend_lambda_{create_shared_from_unique(trend_lambda)},
      // Income trend member variables
      trend_type_{trend_type},
      expected_income_trend_{expected_income_trend
                                 ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                                       std::move(*expected_income_trend))
                                 : nullptr},
      expected_income_trend_boxcox_{
          expected_income_trend_boxcox
              ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                    std::move(*expected_income_trend_boxcox))
              : nullptr},
      income_trend_steps_{income_trend_steps
                              ? std::make_shared<std::unordered_map<core::Identifier, int>>(
                                    std::move(*income_trend_steps))
                              : nullptr},
      income_trend_models_{income_trend_models ? std::make_shared<std::vector<LinearModelParams>>(
                                                     std::move(*income_trend_models))
                                               : nullptr},
      income_trend_ranges_{
          income_trend_ranges
              ? std::make_shared<std::vector<core::DoubleInterval>>(std::move(*income_trend_ranges))
              : nullptr},
      income_trend_lambda_{income_trend_lambda ? std::make_shared<std::vector<double>>(
                                                     std::move(*income_trend_lambda))
                                               : nullptr},
      income_trend_decay_factors_{
          income_trend_decay_factors
              ? std::make_shared<std::unordered_map<core::Identifier, double>>(
                    std::move(*income_trend_decay_factors))
              : nullptr},
      // Common member variables
      names_{std::move(names)}, models_{std::move(models)}, ranges_{std::move(ranges)},
      lambda_{std::move(lambda)}, stddev_{std::move(stddev)}, cholesky_{std::move(cholesky)},
      policy_models_{std::move(policy_models)}, policy_ranges_{std::move(policy_ranges)},
      policy_cholesky_{std::move(policy_cholesky)}, info_speed_{info_speed},
      rural_prevalence_{std::move(rural_prevalence)}, income_models_{std::move(income_models)},
      physical_activity_stddev_{physical_activity_stddev},
      physical_activity_models_{physical_activity_models},
      has_physical_activity_models_{!physical_activity_models.empty()},
      // MAHIMA: For income based factor means adjustment - definition retains per-stratum expected
      // tables for create_model().
      income_stratum_expected_tables_{income_stratum_expected_tables},
      income_stratum_adjustment_enabled_{income_stratum_adjustment_enabled},
      adjustment_income_stratum_count_{adjustment_income_stratum_count},
      // Two-stage modeling: Logistic regression for zero probability (optional)
      // Must be initialized before continuous income model fields (declaration order)
      logistic_models_{std::move(logistic_models)},
      // Continuous income model support (FINCH approach) - must be in declaration order
      is_continuous_income_model_{is_continuous_income_model},
      continuous_income_model_{continuous_income_model},
      income_categories_{std::move(income_categories)},
      // Policy optimization flag - Mahima - must be last (declaration order)
      has_active_policies_{has_active_policies} {

    if (names_.empty()) {
        throw core::HgpsException("Risk factor names list is empty");
    }
    if (models_.empty()) {
        throw core::HgpsException("Risk factor model list is empty");
    }
    if (ranges_.empty()) {
        throw core::HgpsException("Risk factor ranges list is empty");
    }
    if (lambda_.empty()) {
        throw core::HgpsException("Risk factor lambda list is empty");
    }
    if (stddev_.empty()) {
        throw core::HgpsException("Risk factor standard deviation list is empty");
    }
    if (!cholesky_.allFinite()) {
        throw core::HgpsException("Risk factor Cholesky matrix contains non-finite values");
    }
    if (policy_models_.empty()) {
        throw core::HgpsException("Intervention policy model list is empty");
    }
    if (policy_ranges_.empty()) {
        throw core::HgpsException("Intervention policy ranges list is empty");
    }
    if (!policy_cholesky_.allFinite()) {
        throw core::HgpsException("Intervention policy Cholesky matrix contains non-finite values");
    }

    // Validate UPF trend parameters only if trend type is UPFTrend
    if (trend_type == hgps::TrendType::UPFTrend) {
        if (trend_models_->empty()) {
            throw core::HgpsException("Time trend model list is empty");
        }
        if (trend_ranges_->empty()) {
            throw core::HgpsException("Time trend ranges list is empty");
        }
        if (trend_lambda_->empty()) {
            throw core::HgpsException("Time trend lambda list is empty");
        }
    }

    // Validate income trend parameters if income trend is enabled
    if (trend_type == hgps::TrendType::IncomeTrend) {
        if (!expected_income_trend_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend is missing");
        }
        if (!expected_income_trend_boxcox_) {
            throw core::HgpsException(
                "Income trend is enabled but expected_income_trend_boxcox is missing");
        }
        if (!income_trend_steps_) {
            throw core::HgpsException("Income trend is enabled but income_trend_steps is missing");
        }
        if (!income_trend_models_) {
            throw core::HgpsException("Income trend is enabled but income_trend_models is missing");
        }
        if (!income_trend_ranges_) {
            throw core::HgpsException("Income trend is enabled but income_trend_ranges is missing");
        }
        if (!income_trend_lambda_) {
            throw core::HgpsException("Income trend is enabled but income_trend_lambda is missing");
        }
        if (!income_trend_decay_factors_) {
            throw core::HgpsException(
                "Income trend is enabled but income_trend_decay_factors is missing");
        }
    }

    // Validate income trend data consistency
    if (income_trend_models_ && income_trend_models_->empty()) {
        throw core::HgpsException("Income trend model list is empty");
    }
    if (income_trend_ranges_ && income_trend_ranges_->empty()) {
        throw core::HgpsException("Income trend ranges list is empty");
    }
    if (income_trend_lambda_ && income_trend_lambda_->empty()) {
        throw core::HgpsException("Income trend lambda list is empty");
    }

    // Validate that all risk factors have income trend parameters
    if (trend_type == hgps::TrendType::IncomeTrend && expected_income_trend_) {
        for (const auto &name : names_) {
            if (!expected_income_trend_->contains(name)) {
                throw core::HgpsException(
                    "One or more expected income trend value is missing for risk factor: " +
                    name.to_string());
            }
            if (!expected_income_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected income trend BoxCox value is "
                                          "missing for risk factor: " +
                                          name.to_string());
            }
            if (!income_trend_steps_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend steps value is missing for risk factor: " +
                    name.to_string());
            }
            if (!income_trend_decay_factors_->contains(name)) {
                throw core::HgpsException(
                    "One or more income trend decay factor is missing for risk factor: " +
                    name.to_string());
            }
        }
    }

    if (rural_prevalence_.empty()) {
        throw core::HgpsException("Rural prevalence mapping is empty");
    }
    if (income_models_.empty()) {
        throw core::HgpsException("Income models mapping is empty");
    }

    // Policy detection is now done earlier in the model parser for better performance

    // Validate UPF trend parameters for all risk factors only if trend type is UPFTrend
    if (trend_type == hgps::TrendType::UPFTrend) {
        for (const auto &name : names_) {
            if (!expected_trend_->contains(name)) {
                throw core::HgpsException("One or more expected trend value is missing");
            }
            if (!expected_trend_boxcox_->contains(name)) {
                throw core::HgpsException("One or more expected trend BoxCox value is missing");
            }
        }
    }
}

std::unique_ptr<RiskFactorModel> StaticLinearModelDefinition::create_model() const {
    return std::make_unique<StaticLinearModel>(
        expected_, expected_trend_, trend_steps_, expected_trend_boxcox_, names_, models_, ranges_,
        lambda_, stddev_, cholesky_, policy_models_, policy_ranges_, policy_cholesky_,
        trend_models_, trend_ranges_, trend_lambda_, info_speed_, rural_prevalence_, income_models_,
        physical_activity_stddev_, trend_type_, expected_income_trend_,
        expected_income_trend_boxcox_, income_trend_steps_, income_trend_models_,
        income_trend_ranges_, income_trend_lambda_, income_trend_decay_factors_,
        is_continuous_income_model_, continuous_income_model_, income_categories_,
        physical_activity_models_, income_stratum_expected_tables_,
        income_stratum_adjustment_enabled_, adjustment_income_stratum_count_, has_active_policies_,
        logistic_models_);
}

} // namespace hgps
