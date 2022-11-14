#pragma once

#include "interfaces.h"
#include "repository.h"
#include "modelinput.h"
#include "runtime_context.h"

namespace hgps {
	class DiseaseModule final : public DiseaseHostModule {

	public:
		DiseaseModule() = delete;
		DiseaseModule(std::map<core::Identifier, std::shared_ptr<DiseaseModel>>&& models);

		SimulationModuleType type() const noexcept override;

		const std::string& name() const noexcept override;

		std::size_t size() const noexcept override;

		bool contains(const core::Identifier& disease_core) const noexcept override;

		std::shared_ptr<DiseaseModel>& operator[](const core::Identifier& disease_core);

		const std::shared_ptr<DiseaseModel>& operator[](const core::Identifier& disease_code) const;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context) override;

		double get_excess_mortality(
			const core::Identifier& disease_code, const Person& entity) const noexcept override;

	private:
		std::map<core::Identifier, std::shared_ptr<DiseaseModel>> models_;
		std::string name_{ "Disease" };
	};

	std::unique_ptr<DiseaseModule> build_disease_module(Repository& repository, const ModelInput& config);
}
