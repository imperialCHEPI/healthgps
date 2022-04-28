#include "repository.h"
#include "converter.h"

#include <fmt/core.h>
#include <stdexcept>

namespace hgps {

	CachedRepository::CachedRepository(core::Datastore& manager)
		: mutex_{}, data_manager_{ manager },
		model_definiton_{}, lite_model_definiton_{}, diseases_info_{}, diseases_{} {}

	bool CachedRepository::register_linear_model_definition(
		const HierarchicalModelType& model_type, HierarchicalLinearModelDefinition&& definition) {
		std::unique_lock<std::mutex> lock(mutex_);
		if (model_definiton_.contains(model_type)) {
			model_definiton_.erase(model_type);
		}

		auto success = model_definiton_.emplace(model_type, std::move(definition));
		return success.second;
	}

	bool CachedRepository::register_lite_linear_model_definition(
		const HierarchicalModelType& model_type, LiteHierarchicalModelDefinition&& definition) {
		std::unique_lock<std::mutex> lock(mutex_);
		if (lite_model_definiton_.contains(model_type)) {
			lite_model_definiton_.erase(model_type);
		}

		auto success = lite_model_definiton_.emplace(model_type, std::move(definition));
		return success.second;
	}

	HierarchicalLinearModelDefinition& CachedRepository::get_linear_model_definition(
		const HierarchicalModelType& model_type) {

		std::scoped_lock<std::mutex> lock(mutex_);
		return model_definiton_.at(model_type);
	}

	LiteHierarchicalModelDefinition& CachedRepository::get_lite_linear_model_definition(
		const HierarchicalModelType& model_type) {

		std::scoped_lock<std::mutex> lock(mutex_);
		return lite_model_definiton_.at(model_type);
	}

	const std::vector<core::DiseaseInfo>& CachedRepository::get_diseases() {
		std::scoped_lock<std::mutex> lock(mutex_);
		if (diseases_info_.empty()) {
			diseases_info_ = data_manager_.get().get_diseases();
		}

		return diseases_info_;
	}

	std::optional<core::DiseaseInfo> CachedRepository::get_disease_info(std::string code) {
		auto code_key = core::to_lower(code);
		auto& all_diseases = get_diseases();
		auto it = std::find_if(all_diseases.cbegin(), all_diseases.cend(),
			[&code_key](const core::DiseaseInfo& other) {
				return other.code == code_key;
			});

		if (it != all_diseases.cend()) {
			return *it;
		}

		return std::optional<core::DiseaseInfo>();
	}

	DiseaseDefinition& CachedRepository::get_disease_definition(
		const core::DiseaseInfo& info, const ModelInput& config) {

		// lock-free multiple readers
		if (diseases_.contains(info.code)) {
			return diseases_.at(info.code);
		}

		std::scoped_lock<std::mutex> lock(mutex_);
		try {
			load_disease_definition(info, config);
			return diseases_.at(info.code);
		}
		catch (const std::exception& ex) {
			throw std::runtime_error(fmt::format("Filed to load disease {} definition, {}", info.code, ex.what()));
		}
	}

	core::Datastore& CachedRepository::manager() noexcept {
		return data_manager_;
	}

	void CachedRepository::clear_cache() noexcept {
		std::unique_lock<std::mutex> lock(mutex_);
		model_definiton_.clear();
		lite_model_definiton_.clear();
		diseases_info_.clear();
		diseases_.clear();
	}

	void CachedRepository::load_disease_definition(
		const core::DiseaseInfo& info, const ModelInput& config) {
		if (diseases_.contains(info.code)) {
			return;
		}

		auto risk_factors = std::vector<MappingEntry>();
		for (int level = 1; level <= config.risk_mapping().max_level(); level++) {
			auto risks = config.risk_mapping().at_level(level);
			risk_factors.insert(risk_factors.end(), risks.begin(), risks.end());
		}

		auto disease_entity = data_manager_.get().get_disease(info, config.settings().country());
		auto disease_table = detail::StoreConverter::to_disease_table(disease_entity);
		auto relative_risks = detail::create_relative_risk(detail::RelativeRiskInfo{
			.disease = info,
			.manager = data_manager_.get(),
			.inputs = config,
			.risk_factors = risk_factors });

		if (info.group != core::DiseaseGroup::cancer) {
			auto definition = DiseaseDefinition(std::move(disease_table),
				std::move(relative_risks.diseases), std::move(relative_risks.risk_factors));
			diseases_.emplace(info.code, definition);
		}
		else {
			auto cancer_param = data_manager_.get().get_disease_parameter(info, config.settings().country());
			auto parameter = detail::StoreConverter::to_disease_parameter(cancer_param);

			auto definition = DiseaseDefinition(std::move(disease_table), std::move(relative_risks.diseases),
				std::move(relative_risks.risk_factors), std::move(parameter));
			diseases_.emplace(info.code, definition);
		}
	}
}
