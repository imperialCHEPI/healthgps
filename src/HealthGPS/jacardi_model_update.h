#pragma once

#include "jacardi_education_lifecycle.h"
#include "risk_factor_model.h"

#include <memory>
#include <string>

namespace hgps {

/// MAHIMA: JACARDI dynamic / yearly pathway (`ModelName`: JacardiModelUpdate).
/// Part B education milestones (age 6 / 19 / 22 / 23–30) after each ageing step.
class JacardiModelUpdate final : public RiskFactorModel {
  public:
    explicit JacardiModelUpdate(std::shared_ptr<const EducationLifecycleTables> education);

    RiskFactorModelType type() const noexcept override;
    std::string name() const noexcept override;

    /// Init is owned by JacardiModel — nothing to do here.
    void generate_risk_factors(RuntimeContext &context) override;

    /// Part B for every active person after ageing.
    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::shared_ptr<const EducationLifecycleTables> education_;
};

class JacardiModelUpdateDefinition final : public RiskFactorModelDefinition {
  public:
    explicit JacardiModelUpdateDefinition(std::shared_ptr<EducationLifecycleTables> education);

    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::shared_ptr<EducationLifecycleTables> education_;
};

} // namespace hgps
