#include "disease.h"
#include "disease_model.h"
#include "converter.h"

namespace hgps {

	DiseaseModule::DiseaseModule(std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>&& models)
		: models_{ models } {}

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
		for (auto& model : models_) {
			model.second->initialise_disease_status(context);
		}

		// Recalculate relative risks once diseases status were generated
		for (auto& model : models_) {
			model.second->initialise_average_relative_risk(context);
		}
	}

	std::unique_ptr<DiseaseModule> build_disease_module(core::Datastore& manager, ModelInput& config)
	{
		// Models must be registered prior to be created.
		auto registry = get_default_disease_model_registry();
		auto models = std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>();

		auto risk_factors = std::vector<MappingEntry>();
		for (int level = 1; level <= config.risk_mapping().max_level(); level++) {
			auto risks = config.risk_mapping().at_level(level);
			risk_factors.insert(risk_factors.end(), risks.begin(), risks.end());
		}

		for (auto& info : config.diseases()) {
			if (!registry.contains(info.code)) {
				continue; // TODO: Throw argument exception.
			}

			auto disease = manager.get_disease(info, config.settings().country());
			auto desease_table = detail::StoreConverter::to_disease_table(disease);

			auto relative_risks = detail::create_relative_risk(detail::RelativeRiskInfo
				{
					.disease = info,
					.manager = manager,
					.inputs = config,
					.risk_factors = risk_factors
				});

			models.emplace(info.code, registry.at(info.code)(info.code,
				std::move(desease_table), std::move(relative_risks), config.settings().age_range()));
		}

		return std::make_unique<DiseaseModule>(std::move(models));
	}
}
