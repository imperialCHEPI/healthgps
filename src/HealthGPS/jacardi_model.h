#pragma once

#include "jacardi_education_lifecycle.h"
#include "risk_factor_model.h"

#include <memory>
#include <string>

namespace hgps {

/// MAHIMA: JACARDI static / init pathway (`ModelName`: JacardiModel).
/// Writes `Person.risk_factors["Education"]` via Part A so Analysis can emit
/// `mean_Education` / `std_Education` when Education is in modelling.risk_factors.
class JacardiModel final : public RiskFactorModel {
  public:
    explicit JacardiModel(std::shared_ptr<const EducationLifecycleTables> education);

    RiskFactorModelType type() const noexcept override;
    std::string name() const noexcept override;

    /// Part A for the full active population at simulation start.
    void generate_risk_factors(RuntimeContext &context) override;

    /// Part A only for people still missing a finite Education (e.g. newborns).
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
