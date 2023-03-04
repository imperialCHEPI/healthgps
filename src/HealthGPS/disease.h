#pragma once

#include "interfaces.h"
#include "repository.h"
#include "modelinput.h"
#include "runtime_context.h"

namespace hgps {
	/// @brief Defines the disease module class to host multiple disease models
	class DiseaseModule final : public DiseaseHostModule {

	public:
		DiseaseModule() = delete;
		/// @brief Initialises a new instance of the DiseaseModule class.
		/// @param models Collection of disease model instances
		DiseaseModule(std::map<core::Identifier, std::shared_ptr<DiseaseModel>>&& models);

		SimulationModuleType type() const noexcept override;

		const std::string& name() const noexcept override;

		std::size_t size() const noexcept override;

		bool contains(const core::Identifier& disease_id) const noexcept override;

		/// @brief Gets the model for a given disease
		/// @param disease_id The disease identifier 
		/// @return The disease model instance
		/// @throws std::out_of_range if the module does not have a model for the specified disease.
		std::shared_ptr<DiseaseModel>& operator[](const core::Identifier& disease_id);

		/// @brief Gets the model for a given disease
		/// @param disease_id The disease identifier 
		/// @return The disease model instance
		/// @throws std::out_of_range if the module does not have a model for the specified disease.
		const std::shared_ptr<DiseaseModel>& operator[](const core::Identifier& disease_id) const;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context) override;

		double get_excess_mortality(
			const core::Identifier& disease_code, const Person& entity) const noexcept override;

	private:
		std::map<core::Identifier, std::shared_ptr<DiseaseModel>> models_;
		std::string name_{ "Disease" };
	};

	/// @brief Builds a new instance of the DiseaseModule using the given data infrastructure
	/// @param repository The data repository instance
	/// @param config The model inputs instance
	/// @return A new DiseaseModule instance
	/// @throws std::out_of_range for unknown disease definition identifier
	std::unique_ptr<DiseaseModule> build_disease_module(Repository& repository, const ModelInput& config);
}
