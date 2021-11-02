#pragma once

#include <nlohmann/json.hpp>

#include "options.h"
#include "riskmodel.h"

using json = nlohmann::json;

//--------------------------------------------------------
// Risk Model JSON serialisation / de-serialisation
//--------------------------------------------------------

// Linear models
void to_json(json& j, const CoefficientInfo& p) {
	j = json{
		{"value", p.value},
		{"stdError", p.std_error},
		{"tValue", p.tvalue},
		{"pValue", p.pvalue} };
}

void from_json(const json& j, CoefficientInfo& p) {
	j.at("value").get_to(p.value);
	j.at("stdError").get_to(p.std_error);
	j.at("tValue").get_to(p.tvalue);
	j.at("pValue").get_to(p.pvalue);
}

void to_json(json& j, const LinearModelInfo& p) {
	j = json{
		{"formula", p.formula},
		{"coefficients", p.coefficients},
		{"residualsStandardDeviation", p.residuals_standard_deviation},
		{"rSquared", p.rsquared} };
}

void from_json(const json& j, LinearModelInfo& p) {
	j.at("formula").get_to(p.formula);
	j.at("coefficients").get_to(p.coefficients);
	j.at("residualsStandardDeviation").get_to(p.residuals_standard_deviation);
	j.at("rSquared").get_to(p.rsquared);
}

// Hierarchical levels
void to_json(json& j, const Array2Info& p) {
	j = json{
		{"rows", p.rows},
		{"cols", p.cols},
		{"data", p.data} };
}

void from_json(const json& j, Array2Info& p) {
	j.at("rows").get_to(p.rows);
	j.at("cols").get_to(p.cols);
	j.at("data").get_to(p.data);
}

void to_json(json& j, const HierarchicalLevelInfo& p) {
	j = json{
		{"variables", p.variables},
		{"m", p.transition},
		{"w", p.inverse_transition},
		{"s", p.residual_distribution},
		{"correlation", p.correlation},
		{"variances", p.variances} };
}

void from_json(const json& j, HierarchicalLevelInfo& p) {
	j.at("variables").get_to(p.variables);
	j.at("m").get_to(p.transition);
	j.at("w").get_to(p.inverse_transition);
	j.at("s").get_to(p.residual_distribution);
	j.at("correlation").get_to(p.correlation);
	j.at("variances").get_to(p.variances);
}

//--------------------------------------------------------
// Options JSON serialisation / de-serialisation
//--------------------------------------------------------

// data file
void to_json(json& j, const FileInfo& p) {
	j = json{
		{"name", p.name},
		{"format", p.format},
		{"delimiter", p.delimiter},
		{"columns", p.columns} };
}

void from_json(const json& j, FileInfo& p) {
	j.at("name").get_to(p.name);
	j.at("format").get_to(p.format);
	j.at("delimiter").get_to(p.delimiter);
	j.at("columns").get_to(p.columns);
}

// Data Information
void to_json(json& j, const SettingsInfo& p) {
	j = json{
		{"country_code", p.country},
		{"size_fraction", p.size_fraction},
		{"data_linkage", p.data_linkage},
		{"age_range", p.age_range} };
}

void from_json(const json& j, SettingsInfo& p) {
	j.at("country_code").get_to(p.country);
	j.at("size_fraction").get_to(p.size_fraction);
	j.at("data_linkage").get_to(p.data_linkage);
	j.at("age_range").get_to(p.age_range);
}

// SES Model Information
void to_json(json& j, const SESInfo& p) {
	j = json{
		{"update_interval", p.update_interval},
		{"update_max_age", p.update_max_age},
		{"mapping", p.mapping} };
}

void from_json(const json & j, SESInfo& p) {
		j.at("update_interval").get_to(p.update_interval);
		j.at("update_max_age").get_to(p.update_max_age);
		j.at("mapping").get_to(p.mapping);
	}

// Baseline scenario adjustments
void to_json(json& j, const BaselineInfo& p) {
	j = json{
		{"is_enabled", p.is_enabled},
		{"format", p.format},
		{"delimiter", p.delimiter},
		{"encoding", p.encoding},
		{"file_name", p.file_name} };
}

void from_json(const json& j, BaselineInfo& p) {
	j.at("is_enabled").get_to(p.is_enabled);
	j.at("format").get_to(p.format);
	j.at("delimiter").get_to(p.delimiter);
	j.at("encoding").get_to(p.encoding);
	j.at("file_name").get_to(p.file_name);
}

// Risk Factor Modelling
void to_json(json& j, const RiskFactorInfo& p) {
	j = json{
		{"name", p.name},
		{"level", p.level},
		{"proxy", p.proxy},
		{"range", p.range} };
}

void from_json(const json& j, RiskFactorInfo& p) {
	j.at("name").get_to(p.name);
	j.at("level").get_to(p.level);
	j.at("proxy").get_to(p.proxy);
	j.at("range").get_to(p.range);
}

void to_json(json& j, const VariableInfo& p) {
	j = json{
		{"Name", p.name},
		{"Level", p.level},
		{"Factor", p.factor} };
}

void from_json(const json& j, VariableInfo& p) {
	j.at("Name").get_to(p.name);
	j.at("Level").get_to(p.level);
	j.at("Factor").get_to(p.factor);
}

void to_json(json& j, const ModellingInfo& p) {
	j = json{
		{"risk_factors", p.risk_factors},
		{"dynamic_risk_factor", p.dynamic_risk_factor},
		{"models", p.models},
		{"baseline_adjustment", p.baseline_adjustment} };
}

void from_json(const json& j, ModellingInfo& p) {
	j.at("risk_factors").get_to(p.risk_factors);
	j.at("dynamic_risk_factor").get_to(p.dynamic_risk_factor);
	j.at("models").get_to(p.models);
	j.at("baseline_adjustment").get_to(p.baseline_adjustment);
}

void to_json(json& j, const FactorDynamicEquationInfo& p) {
	j = json{
		{"Name", p.name},
		{"Coefficients", p.coefficients},
		{"ResidualsStandardDeviation", p.residuals_standard_deviation} };
}

void from_json(const json& j, FactorDynamicEquationInfo& p) {
	j.at("Name").get_to(p.name);
	j.at("Coefficients").get_to(p.coefficients);
	j.at("ResidualsStandardDeviation").get_to(p.residuals_standard_deviation);
}

// Policy Scenario
void to_json(json& j, const PolicyPeriodInfo& p) {
	j = json{
		{"start_time", p.start_time},
		{"finish_time", p.to_finish_time_str()} };
}

void from_json(const json& j, PolicyPeriodInfo& p) {
	j.at("start_time").get_to(p.start_time);
	if (!j.at("finish_time").is_null() && !j.at("finish_time").empty()) {
		p.finish_time = j.at("finish_time").get<unsigned int>();
	}
}

void to_json(json& j, const PolicyScenarioInfo& p) {
	j = json{
	{"impact_type", p.impact_type},
	{"impacts", p.impacts},
	{"active_period", p.active_period }};
}

void from_json(const json& j, PolicyScenarioInfo& p) {
	j.at("is_enabled").get_to(p.is_enabled);
	j.at("impact_type").get_to(p.impact_type);
	j.at("impacts").get_to(p.impacts);
	j.at("active_period").get_to(p.active_period);
}

// Result information
void to_json(json& j, const ResultInfo& p) {
	j = json{ {"folder", p.folder}};
}

void from_json(const json& j, ResultInfo& p) {
	j.at("folder").get_to(p.folder);
}
