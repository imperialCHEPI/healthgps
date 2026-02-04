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
#include <iostream>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <utility>

namespace { // anonymous namespace

// Helper function to create shared_ptr from unique_ptr before moving
template <typename T> std::shared_ptr<T> create_shared_from_unique(std::unique_ptr<T> &ptr) {
    return ptr ? std::make_shared<T>(*ptr) : nullptr;
}

// Shared thread-local cache for income thresholds (quartiles for 4 categories, tertiles for 3
// categories)
struct IncomeThresholdCache {
    std::vector<double> thresholds; // Quartiles [Q1, Q2, Q3] for 4 categories, or tertiles [T1, T2]
                                    // for 3 categories
    size_t population_size = 0;
    int year = -1;
    std::string income_categories; // "3" or "4" to know which type of thresholds we have
};

// Helper function to get shared thread-local income threshold cache
IncomeThresholdCache &get_income_threshold_cache() {
    thread_local static IncomeThresholdCache cache;
    return cache;
}

} // anonymous namespace

namespace hgps {

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
    bool has_active_policies, const std::vector<LinearModelParams> &logistic_models)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    : RiskFactorAdjustableModel{std::move(expected),       expected_trend, trend_steps, trend_type,
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
    // Continuous: regression (age, gender, region, ethnicity + random) for all. Quartiles/
    // tertiles and category assignment happen AFTER adjustment to factors mean (see below).
    // Categorical: logits + softmax -> 3 categories; no adjustment to factors mean.
    if (is_continuous_income_model_) {
        // Phase 1 only: assign continuous income via regression for all people.
        for (auto &person : context.population()) {
            double continuous_income = calculate_continuous_income(person, context.random());
            person.risk_factors["income"_id] = continuous_income;
            person.income_continuous = continuous_income;
            person.income = core::Income::unknown; // set after adjustment + quartiles below
        }
        // Phase 2 (quartiles/tertiles) and Phase 3 (assign categories) are done after
        // adjust_risk_factors below, so thresholds are computed from adjusted income.
    } else {
        for (auto &person : context.population()) {
            initialise_categorical_income(person, context.random());
        }
        std::cout << " done.";
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
    std::cout << "\nSet " << logistic_factors_set.size()
              << " logistic factors for simulated mean calculation";

    // MAHIMA: Initial factors-mean adjustment only when user sets
    // risk_factors.adjust_to_factors_mean to true in project_requirements. No hardcoded true/false
    // – we use config only.
    const auto &req = context.inputs().project_requirements();
    if (req.risk_factors.adjust_to_factors_mean) {
        // Build extended list: adds income/PA to adjustement for factors mean only when
        // project_requirements say adjust_to_factors_mean
        auto [extended_factors, extended_ranges] =
            build_extended_factors_list(context, names_, ranges_);

        if (extended_factors.size() > names_.size()) {
            std::vector<std::string> added;
            const core::Identifier income_id("income");
            const core::Identifier pa_id("PhysicalActivity");
            auto in_base = [&names = names_](const core::Identifier &id) {
                return std::find(names.begin(), names.end(), id) != names.end();
            };
            if (!in_base(income_id) && std::find(extended_factors.begin(), extended_factors.end(),
                                                 income_id) != extended_factors.end()) {
                added.push_back("income");
            }
            if (!in_base(pa_id) && std::find(extended_factors.begin(), extended_factors.end(),
                                             pa_id) != extended_factors.end()) {
                added.push_back("physical_activity");
            }
            std::string list_str;
            for (size_t i = 0; i < added.size(); ++i) {
                if (i > 0)
                    list_str += ", ";
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

    // Continuous income only: after adjustment, clamp to factors-mean range, then assign
    // income_categories (3 or 4).
    if (is_continuous_income_model_) {
        // Clamp income to the min/max expected (factors-mean) range so values stay within range.
        const core::Identifier income_id("income");
        double income_min = std::numeric_limits<double>::max();
        double income_max = std::numeric_limits<double>::lowest();
        const auto age_range = context.age_range();
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            for (auto sex : {core::Gender::male, core::Gender::female}) {
                try {
                    double e = get_expected(context, sex, age, income_id, std::nullopt, false);
                    income_min = std::min(income_min, e);
                    income_max = std::max(income_max, e);
                } catch (...) {
                    // income not in expected table for this (sex, age)
                }
            }
        }
        if (income_min <= income_max) {
            for (auto &person : context.population()) {
                if (!person.is_active())
                    continue;
                if (person.risk_factors.contains(income_id)) {
                    double v = person.risk_factors.at(income_id);
                    v = std::clamp(v, income_min, income_max);
                    person.risk_factors[income_id] = v;
                    if (person.income_continuous > 0.0) {
                        person.income_continuous = v;
                    }
                }
            }
        }

        // Diagnostic: confirm adjusted and clamped income range
        double min_inc = std::numeric_limits<double>::max();
        double max_inc = std::numeric_limits<double>::lowest();
        double sum_inc = 0.0;
        size_t n_inc = 0;
        for (const auto &person : context.population()) {
            if (!person.is_active())
                continue;
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
            std::cout << "\n[INCOME] After adjustment and clamp, income stats: min=" << min_inc
                      << " max=" << max_inc << " mean=" << (sum_inc / static_cast<double>(n_inc))
                      << " n=" << n_inc;
        }
        std::vector<double> thresholds;
        if (income_categories_ == "4") {
            thresholds = calculate_income_quartiles(context.population());
        } else {
            thresholds = calculate_income_tertiles(context.population());
        }
        for (auto &person : context.population()) {
            double continuous_income = person.risk_factors.at("income"_id);
            person.income = convert_income_to_category(continuous_income, thresholds);
        }

        // Print a simple per-category summary (once) showing income ranges and counts
        // after initial adjustment and categorisation.
        struct IncomeCatSummary {
            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::lowest();
            std::size_t count = 0;
        };

        IncomeCatSummary low_sum, lowmid_sum, mid_sum, upmid_sum, high_sum;

        for (const auto &person : context.population()) {
            if (!person.is_active()) {
                continue;
            }
            auto it = person.risk_factors.find("income"_id);
            if (it == person.risk_factors.end()) {
                continue;
            }

            const double v = it->second;
            const auto inc_cat = person.income;

            auto update_summary = [&](IncomeCatSummary &s) {
                s.min = std::min(s.min, v);
                s.max = std::max(s.max, v);
                s.count++;
            };

            switch (inc_cat) {
            case core::Income::low:
                update_summary(low_sum);
                break;
            case core::Income::lowermiddle:
                update_summary(lowmid_sum);
                break;
            case core::Income::middle:
                update_summary(mid_sum);
                break;
            case core::Income::uppermiddle:
                update_summary(upmid_sum);
                break;
            case core::Income::high:
                update_summary(high_sum);
                break;
            default:
                break;
            }
        }

        auto print_summary = [&](const char *label, int cat_value, const IncomeCatSummary &s) {
            if (s.count == 0) {
                return;
            }
            std::cout << "\n[INCOME SUMMARY] Year " << static_cast<int>(context.time_now()) << " - "
                      << label << " (category " << cat_value << "): [" << s.min << " - " << s.max
                      << "], count=" << s.count;
        };

        // For 4-category continuous income: 1=low, 2=lower middle, 3=upper middle, 4=high.
        // For 3-category income, only low (1), middle (2) and high (3) will be populated.
        print_summary("Low income", 1, low_sum);
        print_summary("Lower middle income", 2, lowmid_sum);
        print_summary("Middle income", 2, mid_sum);
        print_summary("Upper middle income", 3, upmid_sum);
        print_summary("High income", income_categories_ == "4" ? 4 : 3, high_sum);
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
        // Trended adjustment: extended list uses project_requirements
        auto [trended_extended_factors, trended_extended_ranges] =
            build_extended_factors_list(context, names_, ranges_, true);
        adjust_risk_factors(context, trended_extended_factors,
                            trended_extended_ranges.empty()
                                ? std::nullopt
                                : OptionalRanges{trended_extended_ranges},
                            true); // true = apply trend to expected values

        // Continuous income only: clamp to factors-mean range, then recalc thresholds and assign
        // categories.
        if (is_continuous_income_model_) {
            const core::Identifier income_id("income");
            double income_min = std::numeric_limits<double>::max();
            double income_max = std::numeric_limits<double>::lowest();
            const auto age_range = context.age_range();
            for (int age = age_range.lower(); age <= age_range.upper(); age++) {
                for (auto sex : {core::Gender::male, core::Gender::female}) {
                    try {
                        double e = get_expected(context, sex, age, income_id, std::nullopt, false);
                        income_min = std::min(income_min, e);
                        income_max = std::max(income_max, e);
                    } catch (...) {
                    }
                }
            }
            if (income_min <= income_max) {
                for (auto &person : context.population()) {
                    if (!person.is_active())
                        continue;
                    if (person.risk_factors.contains(income_id)) {
                        double v = person.risk_factors.at(income_id);
                        v = std::clamp(v, income_min, income_max);
                        person.risk_factors[income_id] = v;
                        if (person.income_continuous > 0.0)
                            person.income_continuous = v;
                    }
                }
            }
            std::vector<double> thresholds;
            if (income_categories_ == "4") {
                thresholds = calculate_income_quartiles(context.population());
            } else {
                thresholds = calculate_income_tertiles(context.population());
            }
            for (auto &person : context.population()) {
                double continuous_income = person.risk_factors.at("income"_id);
                person.income = convert_income_to_category(continuous_income, thresholds);
            }
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
    // HACK: start intervening two years into the simulation.
    bool intervene = (context.scenario().type() == ScenarioType::intervention &&
                      (context.time_now() - context.start_time()) >= 2);

    auto &cache = get_income_threshold_cache();
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
        auto [extended_factors, extended_ranges] =
            build_extended_factors_list(context, names_, ranges_);
        adjust_risk_factors(context, extended_factors,
                            extended_ranges.empty() ? std::nullopt
                                                    : OptionalRanges{extended_ranges},
                            false); // false = initial adjustment, not trended
    }

    // Continuous income: clamp to factors-mean range, then assign categories.
    if (is_continuous_income_model_) {
        const core::Identifier income_id("income");
        double income_min = std::numeric_limits<double>::max();
        double income_max = std::numeric_limits<double>::lowest();
        const auto age_range = context.age_range();
        for (int age = age_range.lower(); age <= age_range.upper(); age++) {
            for (auto sex : {core::Gender::male, core::Gender::female}) {
                try {
                    double e = get_expected(context, sex, age, income_id, std::nullopt, false);
                    income_min = std::min(income_min, e);
                    income_max = std::max(income_max, e);
                } catch (...) {
                }
            }
        }
        if (income_min <= income_max) {
            for (auto &person : context.population()) {
                if (!person.is_active())
                    continue;
                if (person.risk_factors.contains(income_id)) {
                    double v = person.risk_factors.at(income_id);
                    v = std::clamp(v, income_min, income_max);
                    person.risk_factors[income_id] = v;
                    if (person.income_continuous > 0.0)
                        person.income_continuous = v;
                }
            }
        }
        size_t current_pop_size = context.population().size();
        int current_year = static_cast<int>(context.time_now());

        // Recalculate thresholds if cache is empty, population size changed, year changed, or
        // income_categories changed
        if (cache.thresholds.empty() || cache.population_size != current_pop_size ||
            cache.year != current_year || cache.income_categories != income_categories_) {
            if (income_categories_ == "4") {
                cache.thresholds = calculate_income_quartiles(context.population());
            } else {
                // 3 categories: use tertiles
                cache.thresholds = calculate_income_tertiles(context.population());
            }
            cache.population_size = current_pop_size;
            cache.year = current_year;
            cache.income_categories = income_categories_;
        }

        // Now assign income categories using the calculated thresholds
        // This must happen after thresholds are calculated from adjusted income
        for (auto &person : context.population()) {
            if (!person.is_active()) {
                continue;
            }
            if (person.risk_factors.contains("income"_id)) {
                double continuous_income = person.risk_factors.at("income"_id);
                person.income = convert_income_to_category(continuous_income, cache.thresholds);
            }
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
        auto [trended_extended_factors, trended_extended_ranges] =
            build_extended_factors_list(context, names_, ranges_, true);

        adjust_risk_factors(context, trended_extended_factors,
                            trended_extended_ranges.empty()
                                ? std::nullopt
                                : OptionalRanges{trended_extended_ranges},
                            true); // true = apply trend to expected values

        // Continuous income: clamp to factors-mean range, then recalc thresholds and categories
        if (is_continuous_income_model_) {
            const core::Identifier income_id("income");
            double income_min = std::numeric_limits<double>::max();
            double income_max = std::numeric_limits<double>::lowest();
            const auto age_range = context.age_range();
            for (int age = age_range.lower(); age <= age_range.upper(); age++) {
                for (auto sex : {core::Gender::male, core::Gender::female}) {
                    try {
                        double e = get_expected(context, sex, age, income_id, std::nullopt, false);
                        income_min = std::min(income_min, e);
                        income_max = std::max(income_max, e);
                    } catch (...) {
                    }
                }
            }
            if (income_min <= income_max) {
                for (auto &person : context.population()) {
                    if (!person.is_active())
                        continue;
                    if (person.risk_factors.contains(income_id)) {
                        double v = person.risk_factors.at(income_id);
                        v = std::clamp(v, income_min, income_max);
                        person.risk_factors[income_id] = v;
                        if (person.income_continuous > 0.0)
                            person.income_continuous = v;
                    }
                }
            }
            size_t current_pop_size = context.population().size();
            int current_year = static_cast<int>(context.time_now());

            // Always recalculate thresholds after trend adjustment to ensure they're based on
            // trend-adjusted income values
            if (income_categories_ == "4") {
                cache.thresholds = calculate_income_quartiles(context.population());
            } else {
                // 3 categories: use tertiles
                cache.thresholds = calculate_income_tertiles(context.population());
            }
            cache.population_size = current_pop_size;
            cache.year = current_year;
            cache.income_categories = income_categories_;

            // Reassign income categories using the recalculated thresholds
            for (auto &person : context.population()) {
                if (!person.is_active()) {
                    continue;
                }
                if (person.risk_factors.contains("income"_id)) {
                    double continuous_income = person.risk_factors.at("income"_id);
                    person.income = convert_income_to_category(continuous_income, cache.thresholds);
                }
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

void StaticLinearModel::initialise_factors(RuntimeContext &context, Person &person,
                                           Random &random) const {
    static int factors_count = 0;
    static bool first_call = true;
    static bool summary_printed = false;
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
        // Print 2-stage modeling summary
        std::cout << "\n=== TWO-STAGE MODELING SUMMARY ===";
        std::vector<std::string> two_stage_factors;
        std::vector<std::string> boxcox_only_factors;

        for (const auto &name : names_) {
            bool has_logistic = has_logistic_tracked[name];
            if (has_logistic) {
                two_stage_factors.push_back(name.to_string());
                int zeros = stage1_zero_counts[name];
                int non_zeros = stage2_counts[name];
                std::cout << "\n  " << name.to_string() << " (2-stage): " << zeros
                          << " zeros from Stage 1, " << non_zeros << " non-zeros from Stage 2";
            } else {
                boxcox_only_factors.push_back(name.to_string());
            }
        }

        std::cout
            << "\n\nRisk factors using 2-stage modeling (Stage 1: Logistic, Stage 2: BoxCox): "
            << two_stage_factors.size();
        if (!two_stage_factors.empty()) {
            std::cout << "\n  " << fmt::format("{}", fmt::join(two_stage_factors, ", "));
        }

        std::cout << "\n\nRisk factors using BoxCox-only (no logistic regression): "
                  << boxcox_only_factors.size();
        if (!boxcox_only_factors.empty()) {
            std::cout << "\n  " << fmt::format("{}", fmt::join(boxcox_only_factors, ", "));
        }
        std::cout << "\n======================================";

        summary_printed = true;
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
            const double person_age_d = static_cast<double>(person.age);
            const double cap_d = static_cast<double>(*max_age_opt);
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
    std::ranges::generate(residuals, [&random] { return random.next_normal(0.0, 1.0); });
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

        // Category assignment will happen after income is adjusted and thresholds are recalculated
        // in update_risk_factors(). For now, just set a temporary category (will be updated later).
        // This ensures thresholds are calculated from adjusted income values.
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

// MAHIMA: This is the FINCH approach for continuous income calculation
// In the static_model.json, we have a IncomeModel with intercept, regression values (age,
// gender, region, ethnicity etc) If is_continuous_income_model_ is true, we use this approach-
// the order for this is- calculate continuous income, calculate quartiles, assign category
void StaticLinearModel::initialise_continuous_income(RuntimeContext &context, Person &person,
                                                     Random &random) {
    // FINCH approach: Calculate continuous income, then convert to category
    // Step 1: Calculate continuous income
    double continuous_income = calculate_continuous_income(person, random);

    // Store continuous income in risk factors for future use
    person.risk_factors["income_continuous"_id] = continuous_income;
    person.risk_factors["income"_id] =
        continuous_income; // Also store as "income" for mapping lookup

    // Step 2: Convert to income category based on population quartiles
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
        if (factor_name == "IncomeContinuousStdDev" || factor_name == "stddev" ||
            factor_name == "min" || factor_name == "max") {
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
    auto stddev_it = continuous_income_model_.coefficients.find("IncomeContinuousStdDev"_id);
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
    size_t processed = 0;
    size_t found_count = 0;

    for (const auto &person : population) {
        processed++;

        if (person.is_active()) {
            auto it = person.risk_factors.find("income"_id);
            if (it != person.risk_factors.end()) {
                sorted_incomes.push_back(it->second);
                found_count++;
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
    size_t q1_index = static_cast<size_t>(std::round((n - 1) * 0.25)); // 25th percentile
    size_t q2_index = static_cast<size_t>(std::round((n - 1) * 0.50)); // 50th percentile (median)
    size_t q3_index = static_cast<size_t>(std::round((n - 1) * 0.75)); // 75th percentile

    // Ensure indices are within bounds
    if (q1_index >= n)
        q1_index = n - 1;
    if (q2_index >= n)
        q2_index = n - 1;
    if (q3_index >= n)
        q3_index = n - 1;

    // Set quartile thresholds
    quartile_thresholds[0] = sorted_incomes[q1_index]; // Q1: 25th percentile
    quartile_thresholds[1] = sorted_incomes[q2_index]; // Q2: 50th percentile (median)
    quartile_thresholds[2] = sorted_incomes[q3_index]; // Q3: 75th percentile

    // Q4 is the 100th percentile (maximum value) - not used in thresholds but useful for display
    double q4_value = sorted_incomes.back();

    std::cout << "\n[QUARTILES] Thresholds calculated:\n Q1=" << quartile_thresholds[0]
              << "\n  Q2=" << quartile_thresholds[1] << "\n  Q3=" << quartile_thresholds[2]
              << "\n  Q4=" << q4_value;

    return quartile_thresholds;
}

std::vector<double> StaticLinearModel::calculate_income_tertiles(const Population &population) {
    // Collect all valid continuous income values from the population
    std::vector<double> sorted_incomes;
    sorted_incomes.reserve(population.size());
    size_t processed = 0;
    size_t found_count = 0;

    for (const auto &person : population) {
        processed++;

        if (person.is_active()) {
            auto it = person.risk_factors.find("income"_id);
            if (it != person.risk_factors.end()) {
                sorted_incomes.push_back(it->second);
                found_count++;
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
    size_t t1_index = static_cast<size_t>(std::round((n - 1) * 0.33)); // 33rd percentile
    size_t t2_index = static_cast<size_t>(std::round((n - 1) * 0.67)); // 67th percentile

    // Ensure indices are within bounds
    if (t1_index >= n)
        t1_index = n - 1;
    if (t2_index >= n)
        t2_index = n - 1;

    // Set tertile thresholds
    tertile_thresholds[0] = sorted_incomes[t1_index]; // T1: 33rd percentile
    tertile_thresholds[1] = sorted_incomes[t2_index]; // T2: 67th percentile

    std::cout << "\n[TERTILES] Thresholds calculated: T1=" << tertile_thresholds[0]
              << " (33rd percentile, index " << t1_index << "), T2=" << tertile_thresholds[1]
              << " (67th percentile, index " << t2_index << ")";

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
    bool income_in_base =
        std::find(base_factors.begin(), base_factors.end(), income_id) != base_factors.end();
    bool pa_in_base = std::find(base_factors.begin(), base_factors.end(), PhysicalActivity_id) !=
                      base_factors.end();

    bool add_income =
        for_trended_adjustment ? req.income.trended : req.income.adjust_to_factors_mean;
    if (!income_in_base && req.income.enabled && add_income) {
        try {
            get_expected(context, core::Gender::male, 0, income_id, std::nullopt, false);
            extended_factors.push_back(income_id);
            if (!base_ranges.empty()) {
                extended_ranges.push_back(
                    core::DoubleInterval(0.0, 1e9)); // no effective clamp for continuous income
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
        physical_activity_models_, has_active_policies_, logistic_models_);
}

} // namespace hgps
