#include "disease.h"

namespace hgps {

	DiseaseModule::DiseaseModule(
		std::unordered_map<std::string, std::shared_ptr<DiseaseModel>>&& models) :
		models_{models}	{}

	SimulationModuleType DiseaseModule::type() const {
		return SimulationModuleType::Disease;
	}

	std::string DiseaseModule::name() const {
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
		throw std::logic_error("build_disease_module function not yet implemented.");
	}
}
