#include "disease.h"
#include "disease_registry.h"
#include "converter.h"

namespace hgps {

	DiseaseModule::DiseaseModule(std::map<std::string, std::shared_ptr<DiseaseModel>>&& models)
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

		auto risk_factors = std::vector<MappingEntry>();
		for (int level = 1; level <= config.risk_mapping().max_level(); level++) {
			auto risks = config.risk_mapping().at_level(level);
			risk_factors.insert(risk_factors.end(), risks.begin(), risks.end());
		}

		auto all_diseases = repository.manager().get_diseases();
		for (auto& info : config.diseases()) {
			auto it = std::find_if(all_diseases.begin(), all_diseases.end(), [&info](const auto& obj) {
				return obj.code == info.code;
				});

			if (!registry.contains(info.group) || it == all_diseases.end()) {
				continue; // TODO: Throw argument exception.
			}

			auto disease_entity = repository.manager().get_disease(info, config.settings().country());
			auto disease_table = detail::StoreConverter::to_disease_table(disease_entity);
			auto relative_risks = detail::create_relative_risk(detail::RelativeRiskInfo {
				.disease = info,
				.manager = repository.manager(),
				.inputs = config,
				.risk_factors = risk_factors });

			if (info.group != core::DiseaseGroup::cancer) {
				auto definition = DiseaseDefinition(std::move(disease_table),
					std::move(relative_risks.diseases), std::move(relative_risks.risk_factors));

				models.emplace(info.code, registry.at(info.group)(
					std::move(definition), config.settings().age_range()));
				continue;
			}

			auto cancer_param = repository.manager().get_disease_parameter(info, config.settings().country());
			auto parameter = detail::StoreConverter::to_disease_parameter(cancer_param);

			auto definition = DiseaseDefinition(std::move(disease_table), std::move(relative_risks.diseases),
				std::move(relative_risks.risk_factors), std::move(parameter));

			models.emplace(info.code, registry.at(info.group)(
				std::move(definition), config.settings().age_range()));
		}

		return std::make_unique<DiseaseModule>(std::move(models));
	}
}
