#pragma once
#include "HealthGPS.Core/interval.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Data structures containing model parameters and configuration options
 *
 * POCO stands for "plain old class object". These structs represent data structures
 * which are contained in JSON-formatted configuration files.
 */
namespace hgps::input {
//! Information about a data file to be loaded
struct FileInfo {
    std::filesystem::path name;
    std::string format;
    std::string delimiter;
    std::map<std::string, std::string> columns;

    auto operator<=>(const FileInfo &rhs) const = default;
};

//! Experiment's population settings
struct SettingsInfo {
    std::string country;
    hgps::core::IntegerInterval age_range;
    float size_fraction{};

    auto operator<=>(const SettingsInfo &rhs) const = default;
};

//! Socio-economic status (SES) model inputs
struct SESInfo {
    std::string function;
    std::vector<double> parameters;

    auto operator<=>(const SESInfo &) const = default;
};

// MAHIMA: Income quintile factor means adjustment (see income_quintile_factor_means_plan.md; Phase
// 2+ simulation behaviour).
//! One pair of factors-mean CSV paths for a single income stratum (e.g. one quintile).
//! Used when income_stratum_factors_mean.enabled is true (Phase 2+ simulation behaviour).
struct IncomeStratumFactorsMeanStratumEntry {
    /// Stable label for logs and for mapping persons to this stratum (e.g. "Quintile1").
    std::string id;
    std::filesystem::path factorsmean_male;
    std::filesystem::path factorsmean_female;

    auto operator<=>(const IncomeStratumFactorsMeanStratumEntry &rhs) const = default;
};

// MAHIMA: Income quintile factor means adjustment (see income_quintile_factor_means_plan.md; Phase
// 2+ simulation behaviour).
//! Optional income-stratum factors-mean adjustment: extra male/female tables per stratum.
//! When disabled, the simulator uses only file_names.factorsmean_male/female (legacy behaviour).
//! Final output income bucket count remains in project_requirements.income.categories ("3"/"4").
struct IncomeStratumFactorsMeanConfig {
    bool enabled{false};
    /// Rank buckets formed from continuous income for adjustment (N). Must equal strata.size()
    /// when enabled. Independent of project_requirements income.categories (final person.income).
    // MAHIMA: Stored as std::size_t (not int) because N is a non-negative count comparable to
    // strata.size(); avoids signed/unsigned comparisons and satisfies clang-tidy
    // modernize-use-integer-sign-comparison in validation code.
    std::size_t adjustment_income_stratum_count{0};
    std::vector<IncomeStratumFactorsMeanStratumEntry> strata;

    auto operator<=>(const IncomeStratumFactorsMeanConfig &rhs) const = default;
};

//! Baseline adjustment information
struct BaselineInfo {
    std::string format;
    std::string delimiter;
    std::string encoding;
    std::map<std::string, std::filesystem::path> file_names;
    /// Optional: per-stratum factors-mean files for RF/PA adjustment (see plan
    /// income_quintile_factor_means_plan.md; Phase 1 = parse only).
    IncomeStratumFactorsMeanConfig income_stratum_factors_mean;

    auto operator<=>(const BaselineInfo &rhs) const = default;
};

//! Information about a health risk factor
struct RiskFactorInfo {
    std::string name;
    int level{};
    std::optional<hgps::core::DoubleInterval> range;

    auto operator<=>(const RiskFactorInfo &rhs) const = default;
};

//! User-defined model and parameter information
struct ModellingInfo {
    std::vector<RiskFactorInfo> risk_factors;
    std::unordered_map<std::string, std::filesystem::path> risk_factor_models;
    BaselineInfo baseline_adjustment;
    /// Calendar year when intervention policies start (e.g. 2024). If 0, uses start_time + 2 for
    /// backward compatibility.
    unsigned int policy_start_year{0};
};

// MAHIMA: Per-person CSV output for same-person tracking across baseline/intervention (by ID).
//! Optional config for individual ID tracking CSV: filter by age, gender, region, ethnicity,
//! risk factors, years, scenario; output columns include id, demographics, selected risk factors.
struct IndividualIdTrackingConfig {
    bool enabled{false};
    std::optional<int> age_min;
    std::optional<int> age_max;
    std::string gender{"all"}; // "male" | "female" | "all"
    std::vector<std::string> regions{};
    std::vector<std::string> ethnicities{};
    std::vector<std::string> risk_factors{}; // empty = all from mapping
    std::vector<int> years{};                // empty = all years
    std::string scenarios{"both"};           // "baseline" | "intervention" | "both"

    auto operator<=>(const IndividualIdTrackingConfig &rhs) const = default;
};

//! Experiment output folder and file information
struct OutputInfo {
    unsigned int comorbidities{};
    std::string folder{};
    std::string file_name{};
    std::optional<IndividualIdTrackingConfig> individual_id_tracking;

    auto operator<=>(const OutputInfo &rhs) const = default;
};

//! Information about the period over which a policy is applied
struct PolicyPeriodInfo {
    int start_time{};
    std::optional<int> finish_time;

    std::string to_finish_time_str() const {
        if (finish_time.has_value()) {
            return std::to_string(finish_time.value());
        }

        return "null";
    }

    auto operator<=>(const PolicyPeriodInfo &rhs) const = default;
};

//! Information about policy impacts
struct PolicyImpactInfo {
    std::string risk_factor{};
    double impact_value{};
    unsigned int from_age{};
    std::optional<unsigned int> to_age{};
    std::string to_age_str() const {
        if (to_age.has_value()) {
            return std::to_string(to_age.value());
        }

        return "null";
    }

    auto operator<=>(const PolicyImpactInfo &rhs) const = default;
};

//! Extra adjustments made to a policy
struct PolicyAdjustmentInfo {
    std::string risk_factor{};
    double value{};

    auto operator<=>(const PolicyAdjustmentInfo &rhs) const = default;
};

//! Information about an active policy intervention
struct PolicyScenarioInfo {
    std::string identifier{};
    PolicyPeriodInfo active_period;
    std::vector<double> dynamics;
    std::vector<double> coefficients;
    std::vector<double> coverage_rates;
    std::optional<unsigned int> coverage_cutoff_time{};
    std::optional<unsigned int> child_cutoff_age{};
    std::vector<PolicyAdjustmentInfo> adjustments;
    std::string impact_type{};
    std::vector<PolicyImpactInfo> impacts;

    std::string to_coverage_cutoff_time_str() const {
        if (coverage_cutoff_time.has_value()) {
            return std::to_string(coverage_cutoff_time.value());
        }

        return "null";
    }

    std::string to_child_cutoff_age_str() const {
        if (child_cutoff_age.has_value()) {
            return std::to_string(child_cutoff_age.value());
        }

        return "null";
    }

    auto operator<=>(const PolicyScenarioInfo &rhs) const = default;
};

//! Population Impact Fraction (PIF) configuration
struct PIFInfo {
    bool enabled{};
    std::string data_root_path;
    std::string risk_factor;
    std::string scenario;

    auto operator<=>(const PIFInfo &rhs) const = default;
};

//! Per-project requirements: demographics, income, PA, risk factors, trend, two-stage.
//! Code uses these flags instead of project-specific hacks. Optional in config; defaults if absent.
struct ProjectRequirements {
    struct Demographics {
        bool age{true};
        bool gender{true};
        bool region{false};
        bool ethnicity{false};
        /// Optional. If set and > 0, cap age to this value in linear models (age/age2/age3). Else
        /// no cap.
        std::optional<int> max_age_for_linear_models;
    } demographics;

    struct Income {
        bool enabled{true};
        std::string type{"categorical"}; // "continuous" | "categorical"
        std::string categories{"3"};     // "3" | "4"
        bool adjust_to_factors_mean{false};
        bool trended{false};
        /// When true, write income-based CSV files (categorize results by income_category). When
        /// false, do not.
        bool income_based_csv_output{true};
    } income;

    struct PhysicalActivity {
        bool enabled{true};
        std::string type{"simple"}; // "simple" | "continuous"
        bool adjust_to_factors_mean{false};
        bool trended{false};
    } physical_activity;

    struct RiskFactors {
        bool adjust_to_factors_mean{true};
        bool trended{true};
    } risk_factors;

    struct Trend {
        bool enabled{false};
        std::string type{"null"}; // "null" | "trend" | "upf_trend" | "income_trend"
    } trend;

    struct TwoStage {
        bool use_logistic{false};
        std::string logistic_file;
    } two_stage;
};
} // namespace hgps::input
