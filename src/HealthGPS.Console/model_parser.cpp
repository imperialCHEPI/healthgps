#include "model_parser.h"

#include "csvparser.h"
#include "jsonparser.h"

#include "HealthGPS.Core/scoped_timer.h"

#include <fmt/core.h>
#include <fmt/color.h>
#include <fstream>

#if USE_TIMER
#define MEASURE_FUNCTION()                                                     \
  hgps::core::ScopedTimer timer { __func__ }
#else
#define MEASURE_FUNCTION()
#endif

using namespace hgps;

hgps::BaselineAdjustment load_baseline_adjustments(const BaselineInfo& info)
{
	MEASURE_FUNCTION();
	auto& male_filename = info.file_names.at("factorsmean_male");
	auto& female_filename = info.file_names.at("factorsmean_female");

	try {

		if (core::case_insensitive::equals(info.format, "CSV")) {
			auto data = std::map<core::Gender, std::map<std::string, std::vector<double>>>{};
			data.emplace(core::Gender::male, load_baseline_csv(male_filename, info.delimiter));
			data.emplace(core::Gender::female, load_baseline_csv(female_filename, info.delimiter));
			return BaselineAdjustment{ FactorAdjustmentTable{ std::move(data) } };
		}
		else {
			throw std::logic_error("Unsupported file format: " + info.format);
		}
	}
	catch (const std::exception& ex) {
		fmt::print(fg(fmt::color::red),
			"Failed to parse adjustment file: {} or {}. {}\n", male_filename, female_filename, ex.what());
		throw;
	}
}

HierarchicalLinearModelDefinition load_static_risk_model_definition(std::string model_filename)
{
	MEASURE_FUNCTION();
	std::map<int, HierarchicalLevel> levels;
	std::unordered_map<std::string, LinearModel> models;
	std::ifstream ifs(model_filename, std::ifstream::in);
	if (ifs) {
		try {
			auto opt = json::parse(ifs);
			HierarchicalModelInfo model_info;
			model_info.models = opt["models"].get<std::unordered_map<std::string, LinearModelInfo>>();
			model_info.levels = opt["levels"].get<std::unordered_map<std::string, HierarchicalLevelInfo>>();

			for (auto& model_item : model_info.models) {
				auto& at = model_item.second;

				std::unordered_map<std::string, Coefficient> coeffs;
				for (auto& pair : at.coefficients) {
					coeffs.emplace(core::to_lower(pair.first), Coefficient{
							.value = pair.second.value,
							.pvalue = pair.second.pvalue,
							.tvalue = pair.second.tvalue,
							.std_error = pair.second.std_error
						});
				}

				models.emplace(core::to_lower(model_item.first), LinearModel{
					.coefficients = coeffs,
					.residuals_standard_deviation = at.residuals_standard_deviation,
					.rsquared = at.rsquared
					});
			}

			for (auto& level_item : model_info.levels) {
				auto& at = level_item.second;
				std::unordered_map<std::string, int> col_names;
				auto variables_count = static_cast<int>(at.variables.size());
				for (auto i = 0; i < variables_count; i++) {
					col_names.emplace(core::to_lower(at.variables[i]), i);
				}

				levels.emplace(std::stoi(level_item.first), HierarchicalLevel{
					.variables = col_names,
					.transition = core::DoubleArray2D(
						at.transition.rows, at.transition.cols, at.transition.data),

					.inverse_transition = core::DoubleArray2D(at.inverse_transition.rows,
						at.inverse_transition.cols, at.inverse_transition.data),

					.residual_distribution = core::DoubleArray2D(at.residual_distribution.rows,
						at.residual_distribution.cols, at.residual_distribution.data),

					.correlation = core::DoubleArray2D(at.correlation.rows,
						at.correlation.cols, at.correlation.data),

					.variances = at.variances
					});
			}
		}
		catch (const std::exception& ex) {
			fmt::print(fg(fmt::color::red),
				"Failed to parse model: {:<7}, file: {}. {}\n",
				"static", model_filename, ex.what());
		}
	}
	else {
		fmt::print(fg(fmt::color::red),
			"Model: {:<7}, file: {} not found.\n", "static", model_filename);
	}

	ifs.close();
	return HierarchicalLinearModelDefinition{ std::move(models), std::move(levels) };
}

LiteHierarchicalModelDefinition load_dynamic_risk_model_info(std::string model_filename)
{
	MEASURE_FUNCTION();
	auto percentage = 0.05;
	std::map<std::string, std::string> variables;
	std::map<core::IntegerInterval, AgeGroupGenderEquation> equations;

	std::ifstream ifs(model_filename, std::ifstream::in);
	if (ifs) {
		try {
			auto opt = json::parse(ifs);
			auto info = LiteHierarchicalModelInfo{};
			opt["BoundaryPercentage"].get_to(info.percentage);
			if (info.percentage > 0.0 && info.percentage < 1.0) {
				percentage = info.percentage;
			}
			else {
				fmt::print(fg(fmt::color::red), "Boundary percentage outside range (0, 1): {}.\n", info.percentage);
			}

			info.variables = opt["Variables"].get<std::vector<VariableInfo>>();
			for (auto& it : opt["Equations"].items()) {
				auto& age_key = it.key();
				info.equations.emplace(age_key, std::map<std::string, std::vector<FactorDynamicEquationInfo>>());

				for (auto& sit : it.value().items()) {
					auto gender_key = sit.key();
					auto gender_funcs = sit.value().get<std::vector<FactorDynamicEquationInfo>>();
					info.equations.at(age_key).emplace(gender_key, gender_funcs);
				}
			}

			for (auto& item : info.variables) {
				variables.emplace(core::to_lower(item.name), core::to_lower(item.factor));
			}

			for (auto& age_grp : info.equations) {
				auto limits = core::split_string(age_grp.first, "-");
				auto age_key = core::IntegerInterval(std::stoi(limits[0].data()), std::stoi(limits[1].data()));
				auto age_equations = AgeGroupGenderEquation{ .age_group = age_key };
				for (auto& gender : age_grp.second) {

					if (core::case_insensitive::equals("male", gender.first)) {
						for (auto& func : gender.second) {
							auto function = FactorDynamicEquation{ .name = func.name };
							function.residuals_standard_deviation = func.residuals_standard_deviation;
							for (auto& coeff : func.coefficients) {
								function.coefficients.emplace(core::to_lower(coeff.first), coeff.second);
							}

							age_equations.male.emplace(core::to_lower(func.name), function);
						}
					}
					else if (core::case_insensitive::equals("female", gender.first)) {
						for (auto& func : gender.second) {
							auto function = FactorDynamicEquation{ .name = func.name };
							function.residuals_standard_deviation = func.residuals_standard_deviation;
							for (auto& coeff : func.coefficients) {
								function.coefficients.emplace(core::to_lower(coeff.first), coeff.second);
							}

							age_equations.female.emplace(core::to_lower(func.name), function);
						}
					}
					else {
						fmt::print(fg(fmt::color::red),
							"Unknown model gender type: {}.\n", gender.first);
					}
				}

				equations.emplace(age_key, std::move(age_equations));
			}
		}
		catch (const std::exception& ex) {
			fmt::print(fg(fmt::color::red),
				"Failed to parse model: {:<7}, file: {}. {}\n",
				"static", model_filename, ex.what());
		}
	}
	else {
		fmt::print(fg(fmt::color::red),
			"Model: {:<7}, file: {} not found.\n", "static", model_filename);
	}

	ifs.close();
	return LiteHierarchicalModelDefinition{
		std::move(equations), std::move(variables), percentage };
}

void register_risk_factor_model_definitions(hgps::CachedRepository& repository,
	const ModellingInfo& info, const SettingsInfo& settings)
{
	MEASURE_FUNCTION();
	for (auto& model : info.risk_factor_models) {
		HierarchicalModelType model_type;
		if (core::case_insensitive::equals(model.first, "static")) {
			model_type = HierarchicalModelType::Static;
			auto model_definition = load_static_risk_model_definition(model.second);
			repository.register_linear_model_definition(model_type, std::move(model_definition));
		}
		else if (core::case_insensitive::equals(model.first, "dynamic")) {
			model_type = HierarchicalModelType::Dynamic;
			auto model_definition = load_dynamic_risk_model_info(model.second);
			repository.register_lite_linear_model_definition(model_type, std::move(model_definition));
		}
		else {
			fmt::print(fg(fmt::color::red), "Unknown hierarchical model type: {}.\n", model.first);
			continue;
		}
	}

	auto adjustment = load_baseline_adjustments(info.baseline_adjustment);
	auto age_range = core::IntegerInterval(settings.age_range.front(), settings.age_range.back());
	auto max_age = static_cast<std::size_t>(age_range.upper());
	for (auto& table : adjustment.values) {
		for (auto& item : table.second) {
			if (item.second.size() <= max_age) {
				fmt::print(fg(fmt::color::red), "Baseline adjustment files data must cover age range: [{}].\n",
					age_range.to_string());
				throw std::invalid_argument("Baseline adjustment file must cover the required age range.");
			}
		}
	}

	repository.register_baseline_adjustment_definition(std::move(adjustment));
}
