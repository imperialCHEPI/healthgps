#pragma once

#include "interfaces.h"
#include "repository.h"
#include "modelinput.h"
#include "runtime_context.h"

namespace hgps {
	class DiseaseModule final : public DiseaseHostModule {

	public:
		DiseaseModule() = delete;
		DiseaseModule(std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>&& models);

		SimulationModuleType type() const noexcept override;

		std::string name() const noexcept override;

		std::size_t size() const noexcept;

		bool contains(std::string disease_code) const noexcept;

		std::shared_ptr<DiseaseModel>& operator[](std::string disease_code);

		const std::shared_ptr<DiseaseModel>& operator[](std::string disease_code) const;

		void initialise_population(RuntimeContext& context) override;

		void update_population(RuntimeContext& context) override;

		double get_excess_mortality(
			const std::string disease_code, const Person& entity) const noexcept override;

	private:
		std::unordered_map<std::string, std::shared_ptr<DiseaseModel>> models_;
	};

	std::unique_ptr<DiseaseModule> build_disease_module(Repository& repository, const ModelInput& config);
}
