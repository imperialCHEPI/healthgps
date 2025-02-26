#include "datatable.h"
#include "string_util.h"
#include <fmt/format.h>

#include <sstream>
#include <stdexcept>

namespace hgps::core {

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

std::unordered_map<Region, double> DataTable::get_region_distribution(int age,
                                                                      Gender gender) const {
    // Get coefficients from demographic models
    const auto &coeffs = get_demographic_coefficients("region.probabilities");

    // Calculate probabilities based on age and gender using the coefficients
    std::unordered_map<Region, double> probabilities;
    double total = 0.0;

    // Use the coefficients to calculate probability for each region
    for (const auto &region :
         {Region::England, Region::Wales, Region::Scotland, Region::NorthernIreland}) {
        double prob = calculate_probability(coeffs, age, gender, region);
        probabilities[region] = prob;
        total += prob;
    }

    // Normalize probabilities to sum to 1
    for (auto &[region, prob] : probabilities) {
        prob /= total;
    }

    return probabilities;
}

std::unordered_map<Ethnicity, double> DataTable::get_ethnicity_distribution(int age, Gender gender,
                                                                            Region region) const {
    // Similar implementation but using ethnicity coefficients
    const auto &coeffs = get_demographic_coefficients("ethnicity.probabilities");

    std::unordered_map<Ethnicity, double> probabilities;
    double total = 0.0;

    for (const auto &ethnicity :
         {Ethnicity::White, Ethnicity::Asian, Ethnicity::Black, Ethnicity::Others}) {
        double prob = calculate_probability(coeffs, age, gender, region, ethnicity);
        probabilities[ethnicity] = prob;
        total += prob;
    }

    // Normalize
    for (auto &[ethnicity, prob] : probabilities) {
        prob /= total;
    }

    return probabilities;
}

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
                                        std::optional<Ethnicity> ethnicity) const {
    // Calculate base probability using continuous age
    double prob = age * coeffs.age_coefficient;

    // Add gender effect
    prob += coeffs.gender_coefficients.at(gender);

    // Add region effect if calculating ethnicity probability
    if (ethnicity.has_value()) {
        prob += coeffs.region_coefficients.at(region);
    }

    // Convert to probability using logistic function
    return 1.0 / (1.0 + std::exp(-prob));
}

void DataTable::load_demographic_coefficients(const nlohmann::json &config) {
    // Load region probabilities
    DemographicCoefficients region_coeffs{};
    const auto &region_config =
        config["modelling"]["demographic_models"]["region"]["probabilities"]["coefficients"];

    region_coeffs.age_coefficient = region_config["age"].get<double>();
    region_coeffs.gender_coefficients[Gender::male] = region_config["gender"]["male"].get<double>();
    region_coeffs.gender_coefficients[Gender::female] =
        region_config["gender"]["female"].get<double>();

    demographic_coefficients_["region.probabilities"] = std::move(region_coeffs);

    // Load ethnicity probabilities
    DemographicCoefficients ethnicity_coeffs{};
    const auto &ethnicity_config =
        config["modelling"]["demographic_models"]["ethnicity"]["probabilities"]["coefficients"];

    ethnicity_coeffs.age_coefficient = ethnicity_config["age"].get<double>();
    ethnicity_coeffs.gender_coefficients[Gender::male] =
        ethnicity_config["gender"]["male"].get<double>();
    ethnicity_coeffs.gender_coefficients[Gender::female] =
        ethnicity_config["gender"]["female"].get<double>();

    // Load region coefficients for ethnicity
    const auto &region_probs = ethnicity_config["region"];
    ethnicity_coeffs.region_coefficients[Region::England] = region_probs["England"].get<double>();
    ethnicity_coeffs.region_coefficients[Region::Wales] = region_probs["Wales"].get<double>();
    ethnicity_coeffs.region_coefficients[Region::Scotland] = region_probs["Scotland"].get<double>();
    ethnicity_coeffs.region_coefficients[Region::NorthernIreland] =
        region_probs["NorthernIreland"].get<double>();

    demographic_coefficients_["ethnicity.probabilities"] = std::move(ethnicity_coeffs);
}

} // namespace hgps::core

std::ostream &operator<<(std::ostream &stream, const hgps::core::DataTable &table) {
    stream << table.to_string();
    return stream;
}
