#pragma once

struct Coefficient {
	double value{};
	double pvalue{};
	double tvalue{};
	double std_error{};
};

struct LinearModel {
	std::string formula;
	std::unordered_map<std::string, Coefficient> coefficients;
	std::vector<double> fitted_values;
	std::vector<double> residuals;
	double rsquared{};
};

struct Array2Dto {
	int rows{};
	int cols{};
	std::vector<double> data;
};

struct HierarchicalLevel {
	Array2Dto s;
	Array2Dto w;
	Array2Dto m;
	Array2Dto correlation;
	std::vector<double> variances;
};

struct HierarchicalModel {
	std::unordered_map<std::string, LinearModel> models;
	std::unordered_map<std::string, HierarchicalLevel> levels;
};
