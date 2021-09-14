#pragma once

struct CoefficientInfo {
	double value{};
	double pvalue{};
	double tvalue{};
	double std_error{};
};

struct LinearModelInfo {
	std::string formula;
	std::unordered_map<std::string, CoefficientInfo> coefficients;
	std::vector<double> fitted_values;
	std::vector<double> residuals;
	double rsquared{};
};

struct Array2Info {
	int rows{};
	int cols{};
	std::vector<double> data;
};

struct HierarchicalLevelInfo {
	std::vector<std::string> variables;
	Array2Info residual_distribution;
	Array2Info inverse_transition;
	Array2Info transition;
	Array2Info correlation;
	std::vector<double> variances;
};

struct HierarchicalModelInfo {
	std::unordered_map<std::string, LinearModelInfo> models;
	std::unordered_map<std::string, HierarchicalLevelInfo> levels;
};
