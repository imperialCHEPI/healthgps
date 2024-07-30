#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "repository.h"
#include "runtime_context.h"

namespace hgps {
/// @brief Defines the disease module container to hold disease models
class DiseaseModule final : public UpdatableModule {

  public:
    DiseaseModule() = delete;

    /// @brief Initialises a new instance of the DiseaseModule class.
    /// @param models Collection of disease model instances
    DiseaseModule(std::map<core::Identifier, std::shared_ptr<DiseaseModel>> &&models);

    /// @brief Gets the module type identifier
    /// @return The module type identifier
    SimulationModuleType type() const noexcept override;

    /// @brief Gets the module name
    /// @return The human-readable module name
    const std::string &name() const noexcept override;

    /// @brief Gets the number of diseases models hosted
    /// @return Number of hosted diseases models
    std::size_t size() const noexcept;

    /// @brief Indicates whether the host contains an disease identified by code.
    /// @param disease_id The disease unique identifier
    /// @return true if the disease is found, otherwise false.
    bool contains(const core::Identifier &disease_id) const noexcept;

    /// @brief Gets the model for a given disease
    /// @param disease_id The disease identifier
    /// @return The disease model instance
    /// @throws std::out_of_range if the module does not have a model for the specified disease.
    std::shared_ptr<DiseaseModel> &operator[](const core::Identifier &disease_id);

    /// @brief Gets the model for a given disease
    /// @param disease_id The disease identifier
    /// @return The disease model instance
    /// @throws std::out_of_range if the module does not have a model for the specified disease.
    const std::shared_ptr<DiseaseModel> &operator[](const core::Identifier &disease_id) const;

    /// @brief Initialises the virtual population status
    /// @param context The simulation run-time context
    void initialise_population(RuntimeContext &context) override;

    /// @brief Updates the virtual population status
    /// @param context The simulation run-time context
    void update_population(RuntimeContext &context) override;

    /// @brief Gets the mortality rate associated with a disease for an individual
    /// @param disease_code The disease unique identifier
    /// @param entity The entity associated with the mortality value
    /// @return the mortality rate value, if found, otherwise zero.
    double get_excess_mortality(const core::Identifier &disease_code,
                                const Person &entity) const noexcept;

  private:
    std::map<core::Identifier, std::shared_ptr<DiseaseModel>> models_;
    std::string name_{"Disease"};
};

/// @brief Builds a new instance of the DiseaseModule using the given data infrastructure
/// @param repository The data repository instance
/// @param config The model inputs instance
/// @return A new DiseaseModule instance
/// @throws std::out_of_range for unknown disease definition identifier
std::unique_ptr<DiseaseModule> build_disease_module(Repository &repository,
                                                    const ModelInput &config);
} // namespace hgps
