#include "datatable.h"
#include "HealthGPS.Input/model_parser.h"
#include "string_util.h"
#include <fmt/format.h>

#include <sstream>
#include <stdexcept>

namespace hgps::core {

// Added- Mahima
namespace {
Region parse_region_internal(const std::string &value) {
    if (value == "England") {
        return Region::England;
    }
    if (value == "Wales") {
        return Region::Wales;
    }
    if (value == "Scotland") {
        return Region::Scotland;
    }
    if (value == "NorthernIreland") {
        return Region::NorthernIreland;
    }
    if (value == "unknown") {
        return Region::unknown;
    }
    throw HgpsException(fmt::format("Unknown region value: {}", value));
}

// Added- Mahima
Ethnicity parse_ethnicity_internal(const std::string &value) {
    if (value == "White") {
        return Ethnicity::White;
    }
    if (value == "Asian") {
        return Ethnicity::Asian;
    }
    if (value == "Black") {
        return Ethnicity::Black;
    }
    if (value == "Others") {
        return Ethnicity::Others;
    }
    if (value == "unknown") {
        return Ethnicity::unknown;
    }
    throw HgpsException(fmt::format("Unknown ethnicity value: {}", value));
}
} // namespace

std::size_t DataTable::num_columns() const noexcept { return columns_.size(); }

std::size_t DataTable::num_rows() const noexcept { return rows_count_; }

std::vector<std::string> DataTable::names() const { return names_; }

void DataTable::add(std::unique_ptr<DataTableColumn> column) {

    std::scoped_lock lk(*sync_mtx_);

    if ((rows_count_ > 0 && column->size() != rows_count_) ||
        (num_columns() > 0 && rows_count_ == 0 && column->size() != rows_count_)) {

        throw std::invalid_argument(
            "Column size mismatch, new columns must have the same size of existing ones.");
    }

    auto column_key = to_lower(column->name());
    if (index_.contains(column_key)) {
        throw std::invalid_argument("Duplicated column name is not allowed.");
    }

    rows_count_ = column->size();
    names_.push_back(column->name());
    index_[column_key] = columns_.size();
    columns_.push_back(std::move(column));
}

const DataTableColumn &DataTable::column(std::size_t index) const { return *columns_.at(index); }

const DataTableColumn &DataTable::column(const std::string &name) const {
    auto lower_name = to_lower(name);
    auto found = index_.find(lower_name);
    if (found != index_.end()) {
        return *columns_.at(found->second);
    }

    throw std::out_of_range(fmt::format("Column name: {} not found.", name));
}

std::optional<std::reference_wrapper<const DataTableColumn>>
DataTable::column_if_exists(const std::string &name) const {
    auto found = index_.find(to_lower(name));
    if (found != index_.end()) {
        return std::cref(*columns_.at(found->second));
    }
    return std::nullopt;
}

std::string DataTable::to_string() const noexcept {
    std::stringstream ss;
    std::size_t longestColumnName = 0;
    for (const auto &col : columns_) {
        longestColumnName = std::max(longestColumnName, col->name().length());
    }

    auto pad = longestColumnName + 4;
    auto width = pad + 28;

    ss << fmt::format("\n Table size: {} x {}\n", num_columns(), num_rows());
    ss << fmt::format("|{:-<{}}|\n", '-', width);
    ss << fmt::format("| {:{}} : {:10} : {:>10} |\n", "Column Name", pad, "Data Type", "# Nulls");
    ss << fmt::format("|{:-<{}}|\n", '-', width);
    for (const auto &col : columns_) {
        ss << fmt::format("| {:{}} : {:10} : {:10} |\n", col->name(), pad, col->type(),
                          col->null_count());
    }

    ss << fmt::format("|{:_<{}}|\n\n", '_', width);

    return ss.str();
}

// Added- Mahima
// To get the region distribution data
std::unordered_map<Region, double> DataTable::get_region_distribution(int age,
                                                                      Gender gender) const {
    // Check if required columns exist
    if (!column_if_exists("region") || !column_if_exists("region_prob")) {
        throw std::runtime_error(
            "Required columns 'region' and/or 'region_prob' not found in data table");
    }

    // Get base probabilities from the data table
    const auto &region_col = column("region");
    const auto &prob_col = column("region_prob");

    std::unordered_map<Region, double> probabilities;
    double total = 0.0;

    // First get base probabilities from the data table
    for (size_t i = 0; i < num_rows(); i++) {
        try {
            auto region_str = std::any_cast<std::string>(region_col.value(i));
            auto base_prob = std::any_cast<double>(prob_col.value(i));
            auto region = parse_region_internal(region_str);
            probabilities[region] = base_prob;
            total += base_prob;
        } catch (const std::bad_any_cast &e) {
            throw std::runtime_error(fmt::format("Invalid data type in row {}: {}", i, e.what()));
        }
    }

    if (total <= 0.0) {
        throw std::runtime_error("Sum of region probabilities must be greater than 0");
    }

    // Normalize base probabilities
    for (auto &[region, prob] : probabilities) {
        prob /= total;
    }

    // Get coefficients from demographic models
    try {
        const auto &coeffs = get_demographic_coefficients("region.probabilities");

        // Calculate adjustment based on age and gender
        for (auto &[region, prob] : probabilities) {
            double adjustment = calculate_probability(coeffs, age, gender, region);
            prob *= adjustment;
        }

        // Normalize final probabilities
        total = 0.0;
        for (const auto &[region, prob] : probabilities) {
            total += prob;
        }

        if (total <= 0.0) {
            throw std::runtime_error("Sum of adjusted region probabilities must be greater than 0");
        }

        for (auto &[region, prob] : probabilities) {
            prob /= total;
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(
            fmt::format("Error applying demographic adjustments: {}", e.what()));
    }

    return probabilities;
}

// Added- Mahima
//  To get the ethnicity distribution data
std::unordered_map<Ethnicity, double> DataTable::get_ethnicity_distribution(int age, Gender gender,
                                                                            Region region) const {
    // Check if required columns exist
    if (!column_if_exists("ethnicity") || !column_if_exists("ethnicity_prob")) {
        throw std::runtime_error(
            "Required columns 'ethnicity' and/or 'ethnicity_prob' not found in data table");
    }

    // Get base probabilities from the data table
    const auto &ethnicity_col = column("ethnicity");
    const auto &prob_col = column("ethnicity_prob");

    std::unordered_map<Ethnicity, double> probabilities;
    double total = 0.0;

    // First get base probabilities from the data table
    for (size_t i = 0; i < num_rows(); i++) {
        try {
            auto ethnicity_str = std::any_cast<std::string>(ethnicity_col.value(i));
            auto base_prob = std::any_cast<double>(prob_col.value(i));
            auto ethnicity = parse_ethnicity_internal(ethnicity_str);
            probabilities[ethnicity] = base_prob;
            total += base_prob;
        } catch (const std::bad_any_cast &e) {
            throw std::runtime_error(fmt::format("Invalid data type in row {}: {}", i, e.what()));
        }
    }

    if (total <= 0.0) {
        throw std::runtime_error("Sum of ethnicity probabilities must be greater than 0");
    }

    // Normalize base probabilities
    for (auto &[ethnicity, prob] : probabilities) {
        prob /= total;
    }

    // Get coefficients from demographic models
    try {
        const auto &coeffs = get_demographic_coefficients("ethnicity.probabilities");

        // First apply standard demographic adjustments using age, gender, and region
        double age_effect = coeffs.age_coefficient * age;
        double gender_effect = 0.0;
        if (gender != Gender::unknown && coeffs.gender_coefficients.contains(gender)) {
            gender_effect = coeffs.gender_coefficients.at(gender);
        }

        double region_effect = 0.0;
        if (region != Region::unknown && coeffs.region_coefficients.contains(region)) {
            region_effect = coeffs.region_coefficients.at(region);
        }

        // Calculate and apply ethnicity-specific effects
        for (auto &[ethnicity, prob] : probabilities) {
            // Basic adjustment factor incorporating age, gender, and region effects
            double base_adjustment = 1.0 + (age_effect + gender_effect + region_effect);

            // Add ethnicity-specific adjustment
            double ethnicity_effect = 0.0;
            if (ethnicity != Ethnicity::unknown &&
                coeffs.ethnicity_coefficients.contains(ethnicity)) {
                ethnicity_effect = coeffs.ethnicity_coefficients.at(ethnicity);
            }

            // Apply the combined adjustment
            prob *= (base_adjustment + ethnicity_effect);
        }

        // Check for zero total probability before normalization
        total = 0.0;
        for (const auto &[ethnicity, prob] : probabilities) {
            total += prob;
        }

        if (total <= 0.0) {
            throw std::runtime_error(
                "Sum of adjusted ethnicity probabilities must be greater than 0");
        }

        // Normalize final probabilities
        for (auto &[ethnicity, prob] : probabilities) {
            prob /= total;
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(
            fmt::format("Error applying demographic adjustments: {}", e.what()));
    }

    return probabilities;
}

// Added- Mahima
// Demographic Coefficients refer to Region and Ethnicity models
DataTable::DemographicCoefficients
DataTable::get_demographic_coefficients(const std::string &model_type) const {
    auto it = demographic_coefficients_.find(model_type);
    if (it != demographic_coefficients_.end()) {
        return it->second;
    }

    throw std::runtime_error(
        fmt::format("Demographic coefficients not found for model type: {}", model_type));
}

double DataTable::calculate_probability(const DemographicCoefficients &coeffs, int age,
                                        Gender gender, Region region,
                                        std::optional<Ethnicity> ethnicity) {
    // Start with base probability of 0.0 for adding effects
    double prob = 0.0;

    // Apply age effect (using additive adjustment)
    double age_effect = coeffs.age_coefficient * age;
    prob += age_effect;

    // Apply gender effect if coefficients exist and gender is valid
    if (gender != Gender::unknown && !coeffs.gender_coefficients.empty()) {
        auto gender_it = coeffs.gender_coefficients.find(gender);
        if (gender_it != coeffs.gender_coefficients.end()) {
            prob += gender_it->second;
        }
    }

    // For ethnicity probabilities (when ethnicity is provided), apply region and ethnicity effects
    if (ethnicity.has_value()) {
        // Apply region effect for ethnicity probabilities if coefficients exist
        if (region != Region::unknown && !coeffs.region_coefficients.empty()) {
            auto region_it = coeffs.region_coefficients.find(region);
            if (region_it != coeffs.region_coefficients.end()) {
                prob += region_it->second;
            }
        }

        // Apply ethnicity effect for ethnicity probabilities if coefficients exist
        if (ethnicity.value() != Ethnicity::unknown && !coeffs.ethnicity_coefficients.empty()) {
            auto ethnicity_it = coeffs.ethnicity_coefficients.find(ethnicity.value());
            if (ethnicity_it != coeffs.ethnicity_coefficients.end()) {
                prob += ethnicity_it->second;
            }
        }
    }
    // For region probabilities (when no ethnicity is provided), apply region effects
    else if (region != Region::unknown && !coeffs.region_coefficients.empty()) {
        auto region_it = coeffs.region_coefficients.find(region);
        if (region_it != coeffs.region_coefficients.end()) {
            prob += region_it->second;
        }
    }

    // Ensure final probability is between 0 and 1
    return std::clamp(prob, 0.0, 1.0);
}

// Added- Mahima
//  Loads the coefficients from the config.json
void DataTable::load_demographic_coefficients(const nlohmann::json &config) {
    // Initialize coefficients with default values
    DemographicCoefficients region_coeffs{};
    region_coeffs.age_coefficient = 0.0;
    DemographicCoefficients ethnicity_coeffs{};
    ethnicity_coeffs.age_coefficient = 0.0;

    // Store the coefficients in the map first with default values
    demographic_coefficients_["region.probabilities"] = region_coeffs;
    demographic_coefficients_["ethnicity.probabilities"] = ethnicity_coeffs;
    // Here it loads the coefficients from the config file
    if (!config.contains("modelling") || !config["modelling"].contains("demographic_models")) {
        return; // Return early with default values if no configuration
    }

    const auto &demographic_models = config["modelling"]["demographic_models"];

    // Load coefficients using helper methods
    load_region_coefficients(demographic_models);
    load_ethnicity_coefficients(demographic_models);
}

void DataTable::load_region_coefficients(const nlohmann::json &demographic_models) {
    // Exit early if required sections don't exist
    if (!demographic_models.contains("region") ||
        !demographic_models["region"].contains("probabilities") ||
        !demographic_models["region"]["probabilities"].contains("coefficients")) {
        return;
    }

    // Get current region coefficients
    auto region_coeffs = demographic_coefficients_["region.probabilities"];
    const auto &region_config = demographic_models["region"]["probabilities"]["coefficients"];

    // Load age coefficient
    if (region_config.contains("age")) {
        region_coeffs.age_coefficient = region_config["age"].get<double>();
    }

    // Load gender coefficients
    if (region_config.contains("gender")) {
        const auto &gender = region_config["gender"];
        if (gender.contains("male")) {
            region_coeffs.gender_coefficients[Gender::male] = gender["male"].get<double>();
        }
        if (gender.contains("female")) {
            region_coeffs.gender_coefficients[Gender::female] = gender["female"].get<double>();
        }
    }

    // Load region-specific coefficients
    if (region_config.contains("region")) {
        const auto &region = region_config["region"];
        if (region.contains("England")) {
            region_coeffs.region_coefficients[Region::England] = region["England"].get<double>();
        }
        if (region.contains("Wales")) {
            region_coeffs.region_coefficients[Region::Wales] = region["Wales"].get<double>();
        }
        if (region.contains("Scotland")) {
            region_coeffs.region_coefficients[Region::Scotland] = region["Scotland"].get<double>();
        }
        if (region.contains("NorthernIreland")) {
            region_coeffs.region_coefficients[Region::NorthernIreland] =
                region["NorthernIreland"].get<double>();
        }
    }

    // Update region coefficients in the map
    demographic_coefficients_["region.probabilities"] = region_coeffs;
}

// Implement the helper methods before load_ethnicity_coefficients
void DataTable::load_ethnicity_region_coefficients(DemographicCoefficients &ethnicity_coeffs,
                                                   const nlohmann::json &region_probs) {
    // Process England
    if (region_probs.contains("England")) {
        ethnicity_coeffs.region_coefficients[Region::England] =
            region_probs["England"].get<double>();
    }

    // Process Wales
    if (region_probs.contains("Wales")) {
        ethnicity_coeffs.region_coefficients[Region::Wales] = region_probs["Wales"].get<double>();
    }

    // Process Scotland
    if (region_probs.contains("Scotland")) {
        ethnicity_coeffs.region_coefficients[Region::Scotland] =
            region_probs["Scotland"].get<double>();
    }

    // Process Northern Ireland
    if (region_probs.contains("NorthernIreland")) {
        ethnicity_coeffs.region_coefficients[Region::NorthernIreland] =
            region_probs["NorthernIreland"].get<double>();
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) - Complexity has been reduced with
// helper methods
void DataTable::load_ethnicity_type_coefficients(DemographicCoefficients &ethnicity_coeffs,
                                                 const nlohmann::json &ethnicity_probs) {
    // Process White ethnicity
    if (ethnicity_probs.contains("White")) {
        ethnicity_coeffs.ethnicity_coefficients[Ethnicity::White] =
            ethnicity_probs["White"].get<double>();
    }

    // Process Asian ethnicity
    if (ethnicity_probs.contains("Asian")) {
        ethnicity_coeffs.ethnicity_coefficients[Ethnicity::Asian] =
            ethnicity_probs["Asian"].get<double>();
    }

    // Process Black ethnicity
    if (ethnicity_probs.contains("Black")) {
        ethnicity_coeffs.ethnicity_coefficients[Ethnicity::Black] =
            ethnicity_probs["Black"].get<double>();
    }

    // Process Others ethnicity
    if (ethnicity_probs.contains("Others")) {
        ethnicity_coeffs.ethnicity_coefficients[Ethnicity::Others] =
            ethnicity_probs["Others"].get<double>();
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) - Complexity has been reduced with
// helper methods
void DataTable::load_ethnicity_coefficients(const nlohmann::json &demographic_models) {
    // Exit early if required sections don't exist
    if (!demographic_models.contains("ethnicity") ||
        !demographic_models["ethnicity"].contains("probabilities") ||
        !demographic_models["ethnicity"]["probabilities"].contains("coefficients")) {
        return;
    }

    // Get current ethnicity coefficients
    auto ethnicity_coeffs = demographic_coefficients_["ethnicity.probabilities"];
    const auto &ethnicity_config = demographic_models["ethnicity"]["probabilities"]["coefficients"];

    // Load age coefficient
    if (ethnicity_config.contains("age")) {
        ethnicity_coeffs.age_coefficient = ethnicity_config["age"].get<double>();
    }

    // Load gender coefficients
    if (ethnicity_config.contains("gender")) {
        const auto &gender = ethnicity_config["gender"];
        if (gender.contains("male")) {
            ethnicity_coeffs.gender_coefficients[Gender::male] = gender["male"].get<double>();
        }
        if (gender.contains("female")) {
            ethnicity_coeffs.gender_coefficients[Gender::female] = gender["female"].get<double>();
        }
    }

    // Use helper methods to load region and ethnicity coefficients
    if (ethnicity_config.contains("region")) {
        load_ethnicity_region_coefficients(ethnicity_coeffs, ethnicity_config["region"]);
    }

    if (ethnicity_config.contains("ethnicity")) {
        load_ethnicity_type_coefficients(ethnicity_coeffs, ethnicity_config["ethnicity"]);
    }

    // Update ethnicity coefficients in the map
    demographic_coefficients_["ethnicity.probabilities"] = ethnicity_coeffs;
}

void DataTable::set_demographic_coefficients(const std::string &model_type,
                                             const DemographicCoefficients &coeffs) {
    demographic_coefficients_[model_type] = coeffs;
}

} // namespace hgps::core

std::ostream &operator<<(std::ostream &stream, const hgps::core::DataTable &table) {
    stream << table.to_string();
    return stream;
}
