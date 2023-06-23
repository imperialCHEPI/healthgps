#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "repository.h"

namespace hgps {

/// @brief Implements the socio-economic status (SES) module
class SESNoiseModule final : public UpdatableModule {
  public:
    /// @brief Initialise a new instance of the SESNoiseModule class.
    SESNoiseModule();

    /// @brief Initialise a new instance of the SESNoiseModule class.
    /// @param parameters The model function parameters
    /// @throws std::invalid_argument for number of parameters mismatch.
    SESNoiseModule(const std::vector<double> &parameters);

    /// @brief Initialise a new instance of the SESNoiseModule class.
    /// @param function The model function identifier
    /// @param parameters The model function parameters
    /// @throws std::invalid_argument for unknown model function or number of parameters mismatch.
    SESNoiseModule(std::string function, const std::vector<double> &parameters);

    SimulationModuleType type() const noexcept override;

    const std::string &name() const noexcept override;

    void initialise_population(RuntimeContext &context) override;

    void update_population(RuntimeContext &context) override;

  private:
    std::string function_;
    std::vector<double> parameters_;
    std::string name_{"SES"};
};

/// @brief Builds a new instance of the SESNoiseModule using the given data infrastructure
/// @param repository The data repository instance
/// @param config The model inputs instance
/// @return A new SESNoiseModule instance
/// @throws std::out_of_range for unknown disease definition identifier
std::unique_ptr<SESNoiseModule> build_ses_noise_module(Repository &repository,
                                                       const ModelInput &config);
} // namespace hgps
