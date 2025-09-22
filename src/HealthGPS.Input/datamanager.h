#pragma once

#include "HealthGPS.Core/api.h"
#include "pif_data.h"

#include <filesystem>
#include <nlohmann/json.hpp>

namespace hgps::input {

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

    /// @brief Initialises a new instance of the hgps::input::DataManager class.
    /// @param data_path The path to the directory containing the data.
    /// @param verbosity The terminal logging verbosity mode to use.
    /// @throws std::invalid_argument if the root directory or index.json is missing.
    /// @throws std::runtime_error for invalid or unsupported index.json file schema version.
    explicit DataManager(std::filesystem::path data_path,
                         VerboseMode verbosity = VerboseMode::none);

    std::vector<Country> get_countries() const override;

    Country get_country(const std::string &alpha) const override;

    std::vector<PopulationItem> get_population(const Country &country) const;

    std::vector<PopulationItem>
    get_population(const Country &country,
                   std::function<bool(unsigned int)> time_filter) const override;

    std::vector<MortalityItem> get_mortality(const Country &country) const;

    std::vector<MortalityItem>
    get_mortality(const Country &country,
                  std::function<bool(unsigned int)> time_filter) const override;

    std::vector<DiseaseInfo> get_diseases() const override;

    DiseaseInfo get_disease_info(const core::Identifier &code) const override;

    DiseaseEntity get_disease(const DiseaseInfo &info, const Country &country) const override;

    std::optional<RelativeRiskEntity>
    get_relative_risk_to_disease(const DiseaseInfo &source,
                                 const DiseaseInfo &target) const override;

    std::optional<RelativeRiskEntity>
    get_relative_risk_to_risk_factor(const DiseaseInfo &source, Gender gender,
                                     const core::Identifier &risk_factor_key) const override;

    /// @copydoc core::Datastore::get_disease_parameter
    /// @throws std::out_of_range for unknown disease parameter file type.
    CancerParameterEntity get_disease_parameter(const DiseaseInfo &info,
                                                const Country &country) const override;

    DiseaseAnalysisEntity get_disease_analysis(const Country &country) const override;

    std::vector<BirthItem> get_birth_indicators(const Country &country) const;

    std::vector<BirthItem>
    get_birth_indicators(const Country &country,
                         std::function<bool(unsigned int)> time_filter) const override;

    std::vector<LmsDataRow> get_lms_parameters() const override;

    /// @brief Gets PIF data for a specific disease
    /// @param disease_info The disease information
    /// @param country The country
    /// @param pif_config PIF configuration from config.json
    /// @return PIF data for the disease, or std::nullopt if not available
    std::optional<PIFData> get_pif_data(const DiseaseInfo& disease_info, 
                                       const Country& country, 
                                       const nlohmann::json& pif_config) const;

  private:
    std::filesystem::path root_;
    VerboseMode verbosity_;
    nlohmann::json index_;

    std::map<int, std::map<Gender, double>>
    load_cost_of_diseases(const Country &country, const nlohmann::json &node,
                          const std::filesystem::path &parent_path) const;

    std::vector<LifeExpectancyItem> load_life_expectancy(const Country &country) const;

    static std::string replace_string_tokens(const std::string &source,
                                             const std::vector<std::string> &tokens);

    static std::map<std::string, std::size_t>
    create_fields_index_mapping(const std::vector<std::string> &column_names,
                                const std::vector<std::string> &fields);

    void notify_warning(std::string_view message) const;

    /// @brief Loads PIF data from CSV file
    /// @param filepath Path to PIF CSV file
    /// @return PIF table loaded from file
    PIFTable load_pif_from_csv(const std::filesystem::path& filepath) const;
    
    /// @brief Constructs PIF data path for a specific disease and scenario
    /// @param disease_code Disease code (e.g., "ischemicheartdisease")
    /// @param pif_config PIF configuration
    /// @return Full path to PIF data folder
    std::filesystem::path construct_pif_path(const std::string& disease_code, 
                                            const nlohmann::json& pif_config) const;
    
    /// @brief Expands environment variables in a path string
    /// @param path Path string that may contain environment variables like ${VAR_NAME}
    /// @return Path with environment variables expanded
    std::string expand_environment_variables(const std::string& path) const;
};

} // namespace hgps::input
