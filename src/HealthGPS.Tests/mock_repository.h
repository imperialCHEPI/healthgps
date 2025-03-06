#pragma once

#include "HealthGPS.Core/datastore.h"
#include "HealthGPS/dummy_model.h"
#include "HealthGPS/repository.h"

// Created- Mahima
namespace hgps {
namespace testing {

// Forward declarations
class MockDatastore;
class MockRepository;

// Mock implementation of Datastore for testing
class MockDatastore final : public core::Datastore {
  public:
    MockDatastore() = default;
    ~MockDatastore() override = default;

    // Prevent copying
    MockDatastore(const MockDatastore &) = delete;
    MockDatastore &operator=(const MockDatastore &) = delete;

    // Allow moving
    MockDatastore(MockDatastore &&) noexcept = default;
    MockDatastore &operator=(MockDatastore &&) noexcept = default;

    std::vector<core::Country> get_countries() const override { return {}; }
    core::Country get_country(const std::string &) const override { return {}; }

    std::vector<core::PopulationItem>
    get_population(const core::Country &, std::function<bool(unsigned int)>) const override {
        return {};
    }

    std::vector<core::MortalityItem>
    get_mortality(const core::Country &, std::function<bool(unsigned int)>) const override {
        return {};
    }

    std::vector<core::DiseaseInfo> get_diseases() const override { return {}; }
    core::DiseaseInfo get_disease_info(const core::Identifier &) const override { return {}; }

    core::DiseaseEntity get_disease(const core::DiseaseInfo &,
                                    const core::Country &) const override {
        return {};
    }

    std::optional<core::RelativeRiskEntity>
    get_relative_risk_to_disease(const core::DiseaseInfo &,
                                 const core::DiseaseInfo &) const override {
        return std::nullopt;
    }

    std::optional<core::RelativeRiskEntity>
    get_relative_risk_to_risk_factor(const core::DiseaseInfo &, core::Gender,
                                     const core::Identifier &) const override {
        return std::nullopt;
    }

    core::CancerParameterEntity get_disease_parameter(const core::DiseaseInfo &,
                                                      const core::Country &) const override {
        return {};
    }

    core::DiseaseAnalysisEntity get_disease_analysis(const core::Country &) const override {
        return {};
    }

    std::vector<core::BirthItem>
    get_birth_indicators(const core::Country &, std::function<bool(unsigned int)>) const override {
        return {};
    }

    std::vector<core::LmsDataRow> get_lms_parameters() const override { return {}; }
};

class MockRepository final : public Repository {
  public:
    MockRepository() = default;
    ~MockRepository() override = default;

    // Prevent copying
    MockRepository(const MockRepository &) = delete;
    MockRepository &operator=(const MockRepository &) = delete;

    // Allow moving
    MockRepository(MockRepository &&) noexcept = default;
    MockRepository &operator=(MockRepository &&) noexcept = default;

    core::Datastore &manager() noexcept override {
        static MockDatastore store;
        return store;
    }

    const RiskFactorModelDefinition &
    get_risk_factor_model_definition(const RiskFactorModelType &model_type) const override {
        static std::vector<core::Identifier> names = {core::Identifier("test_factor")};
        static std::vector<double> values = {0.0};
        static std::vector<double> policy = {0.0};
        static std::vector<int> policy_start = {0};
        static DummyModelDefinition definition(model_type, std::move(names), std::move(values),
                                               std::move(policy), std::move(policy_start));
        return definition;
    }

    const std::vector<core::DiseaseInfo> &get_diseases() override {
        static std::vector<core::DiseaseInfo> diseases;
        return diseases;
    }

    std::optional<core::DiseaseInfo> get_disease_info(core::Identifier /*code*/) override {
        return std::nullopt;
    }

    DiseaseDefinition &get_disease_definition(const core::DiseaseInfo &info,
                                              const ModelInput & /*config*/) override {
        static std::map<std::string, int> measures = {{MeasureKey::prevalence, 0},
                                                      {MeasureKey::mortality, 1},
                                                      {MeasureKey::remission, 2},
                                                      {MeasureKey::incidence, 3}};

        static std::map<int, std::map<core::Gender, DiseaseMeasure>> data = {
            {0,
             {{core::Gender::male, DiseaseMeasure({{0, 0.1}, {1, 0.2}, {2, 0.3}, {3, 0.4}})},
              {core::Gender::female, DiseaseMeasure({{0, 0.1}, {1, 0.2}, {2, 0.3}, {3, 0.4}})}}},
            {1,
             {{core::Gender::male, DiseaseMeasure({{0, 0.1}, {1, 0.2}, {2, 0.3}, {3, 0.4}})},
              {core::Gender::female, DiseaseMeasure({{0, 0.1}, {1, 0.2}, {2, 0.3}, {3, 0.4}})}}}};

        static DiseaseTable disease_table(info, std::move(measures), std::move(data));
        static RelativeRiskTableMap diseases;
        static RelativeRiskLookupMap risk_factors;

        static DiseaseDefinition definition(std::move(disease_table), std::move(diseases),
                                            std::move(risk_factors));
        return definition;
    }

    LmsDefinition &get_lms_definition() override {
        static LmsDataset dataset = {{18,
                                      {{core::Gender::male, LmsRecord{1.0, 22.0, 3.0}},
                                       {core::Gender::female, LmsRecord{1.0, 21.0, 3.0}}}}};
        static LmsDefinition lms(std::move(dataset));
        return lms;
    }
};

} // namespace testing
} // namespace hgps
