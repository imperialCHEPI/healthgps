#include "disease.h"
#include "disease_model.h"

namespace hgps {

	DiseaseModule::DiseaseModule(std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>&& models)
		: models_{models}	{}

	SimulationModuleType DiseaseModule::type() const noexcept {
		return SimulationModuleType::Disease;
	}

	std::string DiseaseModule::name() const noexcept {
		return "Disease";
	}

	std::size_t DiseaseModule::size() const noexcept {
		return models_.size();
	}

	bool DiseaseModule::contains(std::string code) const noexcept {
		return models_.contains(code);
	}

	std::shared_ptr<DiseaseModel> DiseaseModule::operator[](std::string code) {
		return models_.at(code);
	}

	const std::shared_ptr<DiseaseModel> DiseaseModule::operator[](std::string code) const {
		return models_.at(code);
	}

	void DiseaseModule::initialise_population(RuntimeContext& context) {
		throw std::logic_error(
			"DisieaseModule.initialise_population function not yet implemented.");
	}

	std::unique_ptr<DiseaseModule> build_disease_module(core::Datastore& manager, ModelInput& config) 
	{
		// Models must be registered prior to be created.
		auto registry = get_default_disease_model_registry();
		auto models = std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>();

		for (auto& info : config.diseases()) {
			if (!registry.contains(info.code)) {
				continue; // TODO: Throw argument exception.
			}

			auto disease = manager.get_disease(info, config.settings().country());
			auto measures = disease.measures;
			auto data = std::map<int, std::map<core::Gender, DiseaseMeasure>>();
			for (auto& v : disease.items) {
				data[v.age][v.gender] = DiseaseMeasure(v.measures);
			}

			auto table = DiseaseTable(info.name, std::move(measures), std::move(data));
			models.emplace(info.code, registry.at(info.code)(info.code, std::move(table)));
		}

		return std::make_unique<DiseaseModule>(std::move(models));
	}
}
