#include "disease.h"
#include "disease_registry.h"

namespace hgps {

	DiseaseModule::DiseaseModule(std::map<std::string, std::shared_ptr<DiseaseModel>>&& models)
		: models_{ std::move(models) } {}

	SimulationModuleType DiseaseModule::type() const noexcept {
		return SimulationModuleType::Disease;
	}

	std::string DiseaseModule::name() const noexcept {
		return "Disease";
	}

	std::size_t DiseaseModule::size() const noexcept {
		return models_.size();
	}

	bool DiseaseModule::contains(const std::string& disease_code) const noexcept {
		return models_.contains(disease_code);
	}

	std::shared_ptr<DiseaseModel>& DiseaseModule::operator[](const std::string& disease_code) {
		return models_.at(disease_code);
	}

	const std::shared_ptr<DiseaseModel>& DiseaseModule::operator[](const std::string& disease_code) const {
		return models_.at(disease_code);
	}

	void DiseaseModule::initialise_population(RuntimeContext& context) {
		for (auto& model : models_) {
			model.second->initialise_disease_status(context);
		}

		// Recalculate relative risks once diseases status were generated
		for (auto& model : models_) {
			model.second->initialise_average_relative_risk(context);
		}
	}

	void DiseaseModule::update_population(RuntimeContext& context) {
		for (auto& model : models_) {
			model.second->update_disease_status(context);
		}
	}

	double DiseaseModule::get_excess_mortality(
		const std::string& disease_code, const Person& entity) const noexcept {
		if (!models_.contains(disease_code)) {
			return 0.0;
		}

		return models_.at(disease_code)->get_excess_mortality(entity);
	}

	std::unique_ptr<DiseaseModule> build_disease_module(Repository& repository, const ModelInput& config)
	{
		// Models must be registered prior to be created.
		auto registry = get_default_disease_model_registry();
		auto models = std::map<std::string, std::shared_ptr<DiseaseModel>>();

		for (auto& info : config.diseases()) {
			auto other = repository.get_disease_info(info.code);
			if (!registry.contains(info.group) || !other.has_value()) {
				throw std::runtime_error("Unknown disease definition: " + info.code);
			}

			auto& shared_definition = repository.get_disease_definition(info, config);
			models.emplace(info.code, registry.at(info.group)(
				shared_definition, config.settings().age_range()));
		}

		return std::make_unique<DiseaseModule>(std::move(models));
	}
}
