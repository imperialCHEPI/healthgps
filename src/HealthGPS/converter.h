#pragma once
#include "analysis_definition.h"
#include "disease_definition.h"
#include "disease_table.h"
#include "gender_table.h"
#include "life_table.h"
#include "lms_definition.h"
#include "modelinput.h"
#include "relative_risk.h"

#include "HealthGPS.Core/datastore.h"
#include "HealthGPS.Core/poco.h"

namespace hgps {
/// @brief Internal details namespace for private data types and functions
namespace detail {
/// @brief Collate the information for retrieving relative risks for a disease
struct RelativeRiskInfo {
    /// @brief The target disease information
    const core::DiseaseInfo &disease;

    /// @brief Back-end data store manager instance
    core::Datastore &manager;

    /// @brief The model inputs
    const ModelInput &inputs;

    /// @brief The risk factor model mappings
    std::vector<MappingEntry> &risk_factors;
};

/// @brief Back-end data stop POCO types converter to Health-GPS types
class StoreConverter {
  public:
    StoreConverter() = delete;

    static core::Gender to_gender(const std::string &name);

    static DiseaseTable to_disease_table(const core::DiseaseEntity &entity);

    static FloatAgeGenderTable to_relative_risk_table(const core::RelativeRiskEntity &entity);

    static RelativeRiskLookup to_relative_risk_lookup(const core::RelativeRiskEntity &entity);

    static AnalysisDefinition to_analysis_definition(const core::DiseaseAnalysisEntity &entity);

    static LifeTable to_life_table(const std::vector<core::BirthItem> &births,
                                   const std::vector<core::MortalityItem> &deaths);

    static DiseaseParameter to_disease_parameter(const core::CancerParameterEntity &entity);

    static LmsDefinition to_lms_definition(const std::vector<core::LmsDataRow> &dataset);
};

/// @brief Creates the relative risk definition for a disease
/// @param info The relative risk information
/// @return The disease relative risk instance
RelativeRisk create_relative_risk(const RelativeRiskInfo &info);

/// @brief Creates a commutative distribution function (CDF) from a frequency list
/// @tparam TYPE Frequency data type
/// @param frequency The frequency list
/// @return The respective CDF data
template <std::floating_point TYPE>
static std::vector<TYPE> create_cdf(std::vector<TYPE> &frequency) {
    auto sum = std::accumulate(std::cbegin(frequency), std::cend(frequency), 0.0f);
    auto pdf = std::vector<float>(frequency.size());
    if (sum > 0) {
        std::transform(std::cbegin(frequency), std::cend(frequency), pdf.begin(),
                       [&sum](auto &v) { return v / sum; });
    } else {
        auto prob = static_cast<TYPE>(1.0 / (pdf.size() - 1));
        std::fill(pdf.begin(), pdf.end(), prob);
    }

    auto cdf = std::vector<float>(pdf.size());
    cdf[0] = pdf[0];
    for (size_t i = 1; i < pdf.size(); i++) {
        cdf[i] = cdf[i - 1] + pdf[i];
    }

    return cdf;
}
} // namespace detail
} // namespace hgps
