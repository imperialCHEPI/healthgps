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
		{"residuals", p.residuals},
		{"fittedValues", p.fitted_values},
		{"rSquared", p.rsquared} };
}

void from_json(const json& j, LinearModelInfo& p) {
	j.at("formula").get_to(p.formula);
	j.at("coefficients").get_to(p.coefficients);
	j.at("residuals").get_to(p.residuals);
	j.at("fittedValues").get_to(p.fitted_values);
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

// Result information
void to_json(json& j, const ResultInfo& p) {
	j = json{ {"folder", p.folder}};
}

void from_json(const json& j, ResultInfo& p) {
	j.at("folder").get_to(p.folder);
}
