#pragma once

#include "jacardi_education_lifecycle.h"
#include "risk_factor_model.h"

#include <memory>
#include <string>

namespace hgps {

/// MAHIMA: JACARDI static / init pathway (ModelName: JacardiModel).
class JacardiModel final : public RiskFactorModel {
  public:
    explicit JacardiModel(std::shared_ptr<const EducationLifecycleTables> education);

    RiskFactorModelType type() const noexcept override;
    std::string name() const noexcept override;

    /// Part A education for the full population at simulation start.
    void generate_risk_factors(RuntimeContext &context) override;

    /// Part A for people who still lack Education (e.g. newborns).
    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::shared_ptr<const EducationLifecycleTables> education_;
};

class JacardiModelDefinition final : public RiskFactorModelDefinition {
  public:
    explicit JacardiModelDefinition(std::shared_ptr<EducationLifecycleTables> education);

    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::shared_ptr<EducationLifecycleTables> education_;
};

} // namespace hgps
