#pragma once

#include "interfaces.h"
#include "modelinput.h"
#include "runtime_context.h"

namespace hgps {
	class DiseaseModule final : public SimulationModule {

	public:
		DiseaseModule() = delete;
		DiseaseModule(std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>&& models);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		std::size_t size() const noexcept;

		bool contains(std::string code) const noexcept;

		std::shared_ptr<DiseaseModel>& operator[](std::string code);

		const std::shared_ptr<DiseaseModel>& operator[](std::string code) const;

		void initialise_population(RuntimeContext& context);

	private:
		std::unordered_map<std::string, std::shared_ptr<DiseaseModel>> models_;
	};

	std::unique_ptr<DiseaseModule> build_disease_module(core::Datastore& manager, ModelInput& config);
}
