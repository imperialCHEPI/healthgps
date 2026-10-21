#pragma once

#include "jacardi_education_lifecycle.h"
#include "jacardi_employment_static.h"
#include "risk_factor_model.h"

#include <memory>
#include <string>

namespace hgps {

/// MAHIMA: JACARDI dynamic / yearly pathway (`ModelName`: JacardiModelUpdate).
/// Opt-in: Education Part B and/or Employment Part B (first draw + temporary hold).
class JacardiModelUpdate final : public RiskFactorModel {
  public:
    JacardiModelUpdate(std::shared_ptr<const EducationLifecycleTables> education,
                       std::shared_ptr<const EmploymentStaticTables> employment);

    RiskFactorModelType type() const noexcept override;
    std::string name() const noexcept override;

    /// Init is owned by JacardiModel — nothing to do here.
    void generate_risk_factors(RuntimeContext &context) override;

    /// Part B for every active person after ageing (education then employment).
    void update_risk_factors(RuntimeContext &context) override;

  private:
    std::shared_ptr<const EducationLifecycleTables> education_;
    std::shared_ptr<const EmploymentStaticTables> employment_;
};

class JacardiModelUpdateDefinition final : public RiskFactorModelDefinition {
  public:
    JacardiModelUpdateDefinition(std::shared_ptr<EducationLifecycleTables> education,
                                 std::shared_ptr<EmploymentStaticTables> employment);

    std::unique_ptr<RiskFactorModel> create_model() const override;

  private:
    std::shared_ptr<EducationLifecycleTables> education_;
    std::shared_ptr<EmploymentStaticTables> employment_;
};

} // namespace hgps
