#pragma once

#include "HealthGPS.Core/api.h"

#include <filesystem>
#include <nlohmann/json.hpp>

namespace hgps::data {

using namespace hgps::core;

/// @brief Implements Health-GPS back-end data store interface using a file-based storage
///
/// The storage is based on a folder structure and indexed by a file, `index.json`, with
/// a versioned schema defining the storage sub-folders structure, file names, data limits,
/// metadata, and rules for accessing the stored contents.
///
/// @note To avoid storing <em>disease-disease</em> interaction files filled with a `default value`
/// representing <em>no relative risk effect</em>, the `default value` is defined in the index
/// file, and the respective dataset populated at run-time with the `default value` when
/// the an associated file does not exists in the store.
///
/// @sa https://imperialchepi.github.io/healthgps/datamodel for the `Data Model` details.
class DataManager : public Datastore {
  public:
    DataManager() = delete;

    /// @brief Initialises a new instance of the hgps::data::DataManager class.
    /// @param root_directory The store root folder containing the index.json file.
    /// @param verbosity The terminal logging verbosity mode to use.
    /// @throws std::invalid_argument if the root directory or index.json is missing.
    /// @throws std::runtime_error for invalid or unsupported index.json file schema version.
    explicit DataManager(const std::filesystem::path root_directory,
                         VerboseMode verbosity = VerboseMode::none);

    std::vector<Country> get_countries() const override;

    Country get_country(const std::string &alpha) const override;

    std::vector<PopulationItem> get_population(Country country) const;

    std::vector<PopulationItem>
    get_population(Country country,
                   const std::function<bool(const unsigned int &)> time_filter) const override;

    std::vector<MortalityItem> get_mortality(Country country) const;

    std::vector<MortalityItem>
    get_mortality(Country country,
                  const std::function<bool(const unsigned int &)> time_filter) const override;

    std::vector<DiseaseInfo> get_diseases() const override;

    DiseaseInfo get_disease_info(const core::Identifier &code) const override;

    DiseaseEntity get_disease(DiseaseInfo code, Country country) const override;

    RelativeRiskEntity get_relative_risk_to_disease(DiseaseInfo source,
                                                    DiseaseInfo target) const override;

    RelativeRiskEntity
    get_relative_risk_to_risk_factor(DiseaseInfo source, Gender gender,
                                     core::Identifier risk_factor_key) const override;

    /// @copydoc core::Datastore::get_disease_parameter
    /// @throws std::out_of_range for unknown disease parameter file type.
    CancerParameterEntity get_disease_parameter(DiseaseInfo info, Country country) const override;

    DiseaseAnalysisEntity get_disease_analysis(const Country country) const override;

    std::vector<BirthItem> get_birth_indicators(const Country country) const;

    std::vector<BirthItem> get_birth_indicators(
        const Country country,
        const std::function<bool(const unsigned int &)> time_filter) const override;

    std::vector<LmsDataRow> get_lms_parameters() const override;

  private:
    std::filesystem::path root_;
    VerboseMode verbosity_;
    nlohmann::json index_;

    RelativeRiskEntity generate_default_relative_risk_to_disease() const;

    std::map<int, std::map<Gender, double>>
    load_cost_of_diseases(Country country, nlohmann::json node,
                          std::filesystem::path parent_path) const;

    std::vector<LifeExpectancyItem> load_life_expectancy(const Country &country) const;

    std::string replace_string_tokens(std::string source, std::vector<std::string> tokens) const;

    std::map<std::string, std::size_t>
    create_fields_index_mapping(const std::vector<std::string> &column_names,
                                const std::vector<std::string> fields) const;

    void notify_warning(const std::string_view message) const;
};
} // namespace hgps::data