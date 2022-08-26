#include "disease.h"
#include "disease_registry.h"
#include "weight_model.h"
#include "lms_model.h"
#include <execution>

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

		auto& diseases = config.diseases();
		std::mutex m;
		std::for_each(std::execution::par, std::begin(diseases), std::end(diseases),
			[&](auto& info) {
				auto other = repository.get_disease_info(info.code);
				if (!registry.contains(info.group) || !other.has_value()) {
					throw std::runtime_error("Unknown disease definition: " + info.code);
				}

				auto& disease_definition = repository.get_disease_definition(info, config);
				auto& lms_definition = repository.get_lms_definition();
				auto classifier = WeightModel{ LmsModel{ lms_definition } };

				// Sync region
				std::scoped_lock lock(m);
				models.emplace(info.code, registry.at(info.group)(
					disease_definition, std::move(classifier), config.settings().age_range()));
			});

		return std::make_unique<DiseaseModule>(std::move(models));
	}
}
