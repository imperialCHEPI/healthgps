#pragma once

#include "HealthGPS/riskfactor.h"

hgps::HierarchicalLinearModel create_static_test_model() {
	using namespace hgps;

	std::unordered_map<std::string, LinearModel> models;
	models.emplace("SmokingStatus", LinearModel{
		.coefficients = {
			{"Intercept", Coefficient{.value = 0.68105, .pvalue = 0.3633309, .tvalue = 1.024929, .std_error = 0.664488 }},
			{"Gender", Coefficient{.value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913179, .std_error = 0.346626 }},
			{"Age", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }}},
		.fitted_values = {0.66657, -0.00087, 0.66094, -0.002479, 0.003954, 0.672475,-0.000603},
		.residuals = {-0.66657, 0.000871, 0.33905, 0.002479, -0.003954, 0.3275249, 0.000603},
		.rsquared = 0.533 });

	models.emplace("AlcoholConsumption", LinearModel{
	.coefficients = {
		{"Intercept", Coefficient{.value = 6.382514, .pvalue = 0.405953, .tvalue = 0.927959, .std_error = 0.664488 }},
		{"Gender", Coefficient{.value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913179, .std_error = 0.346626 }},
		{"Age", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }}},
	.fitted_values = {0.66657, -0.00087, 0.66094, -0.002479, 0.003954, 0.672475,-0.000603},
	.residuals = {-0.66657, 0.000871, 0.33905, 0.002479, -0.003954, 0.3275249, 0.000603},
	.rsquared = 0.533 });

	models.emplace("BMI", LinearModel{
	.coefficients = {
		{"Intercept", Coefficient{.value = 0.68105, .pvalue = 0.3633309, .tvalue = 1.024929, .std_error = 0.664488 }},
		{"Gender", Coefficient{.value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913179, .std_error = 0.346626 }},
		{"Age", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }},
		{"SmokingStatus", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }},
		{"AlcoholConsumption", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }},},
	.fitted_values = {0.66657, -0.00087, 0.66094, -0.002479, 0.003954, 0.672475,-0.000603},
	.residuals = {-0.66657, 0.000871, 0.33905, 0.002479, -0.003954, 0.3275249, 0.000603},
	.rsquared = 0.533 });

	// Levels
	std::map<int, HierarchicalLevel> levels;
	std::vector<double> tmat_m = { -0.00356059, 3.19407228, 0.30856547, 0.01840364 };
	std::vector<double> itmat_w = { -0.01867165, 3.24058782, 0.31305913, 0.00361245 };
	std::vector<double> rmat_s = { 0.02492, 0.41552, 0.78636, -0.74898, 1.73733, -0.811287, -1.403876,
							    -2.15995, 0.00761, 1.10787, -0.00060, 0.007232, 1.052082, -0.0142448 };
	std::vector<double> corr_mat = { 1.0, -0.0057769, -0.0057769, 1.0 };

	levels.emplace(1, HierarchicalLevel{
		.transition = core::DoubleArray2D(2, 2, tmat_m),
		.inverse_transition = core::DoubleArray2D(2, 2, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 2, rmat_s),
		.correlation = core::DoubleArray2D(2, 2, corr_mat),
		.variances = {0.990721, 0.0092789}
		});

	tmat_m = { 0.5815107 };
	itmat_w = { 1.71965877 };
	rmat_s = { -1.15916E-16, -2.23700428, 0.30300198, 0.817005, 0.9712421, -0.30300198, 0.4487571 };
	corr_mat = { 1.0 };

	levels.emplace(2, HierarchicalLevel{
		.transition = core::DoubleArray2D(1, 1, tmat_m),
		.inverse_transition = core::DoubleArray2D(1, 1, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 1, rmat_s),
		.correlation = core::DoubleArray2D(1, 1, corr_mat),
		.variances = {1.0}
		});

	return HierarchicalLinearModel(std::move(models), std::move(levels));
}

hgps::DynamicHierarchicalLinearModel create_dynamic_test_model() {
	using namespace hgps;

	std::unordered_map<std::string, LinearModel> models;
	models.emplace("SmokingStatus", LinearModel{
		.coefficients = {
			{"Intercept", Coefficient{.value = 0.68105, .pvalue = 0.3633309, .tvalue = 1.024929, .std_error = 0.664488 }},
			{"Year", Coefficient{.value = 0.216438, .pvalue = 0.22585, .tvalue = 1.5199459, .std_error = 0.142398 }},
			{"Gender", Coefficient{.value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913179, .std_error = 0.346626 }},
			{"Age", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }}},
		.fitted_values = {0.66657, -0.00087, 0.66094, -0.002479, 0.003954, 0.672475,-0.000603},
		.residuals = {-0.66657, 0.000871, 0.33905, 0.002479, -0.003954, 0.3275249, 0.000603},
		.rsquared = 0.533 });

	models.emplace("AlcoholConsumption", LinearModel{
	.coefficients = {
		{"Intercept", Coefficient{.value = 6.382514, .pvalue = 0.405953, .tvalue = 0.927959, .std_error = 0.664488 }},
		{"Year", Coefficient{.value = 0.216438, .pvalue = 0.22585, .tvalue = 1.5199459, .std_error = 0.142398 }},
		{"Gender", Coefficient{.value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913179, .std_error = 0.346626 }},
		{"Age", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }}},
	.fitted_values = {0.66657, -0.00087, 0.66094, -0.002479, 0.003954, 0.672475,-0.000603},
	.residuals = {-0.66657, 0.000871, 0.33905, 0.002479, -0.003954, 0.3275249, 0.000603},
	.rsquared = 0.533 });

	models.emplace("BMI", LinearModel{
	.coefficients = {
		{"Intercept", Coefficient{.value = 0.68105, .pvalue = 0.3633309, .tvalue = 1.024929, .std_error = 0.664488 }},
		{"Year", Coefficient{.value = 0.216438, .pvalue = 0.22585, .tvalue = 1.5199459, .std_error = 0.142398 }},
		{"Gender", Coefficient{.value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913179, .std_error = 0.346626 }},
		{"Age", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }},
		{"SmokingStatus", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }},
		{"AlcoholConsumption", Coefficient{.value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023156, .std_error = 0.011576 }},},
	.fitted_values = {0.66657, -0.00087, 0.66094, -0.002479, 0.003954, 0.672475,-0.000603},
	.residuals = {-0.66657, 0.000871, 0.33905, 0.002479, -0.003954, 0.3275249, 0.000603},
	.rsquared = 0.533 });

	// Levels
	std::map<int, HierarchicalLevel> levels;
	std::vector<double> tmat_m = { -0.1111982, 2.8656298, -0.2035489, -0.13297382 };
	std::vector<double> itmat_w = { -0.22233357, -4.7913619, 0.3403359, -0.1859245 };
	std::vector<double> rmat_s = { 0.7783874, 0.941040, 0.21149, -0.731434, 1.296447, -0.9898777, -1.506053,
								   1.4248429, -1.649321, -0.4529579, 0.1949148, 0.681437, -0.971885, 0.772969 };
	std::vector<double> corr_mat = { 1.0, -0.43822772, -0.43822772, 1.0 };

	levels.emplace(1, HierarchicalLevel{
		.transition = core::DoubleArray2D(2, 2, tmat_m),
		.inverse_transition = core::DoubleArray2D(2, 2, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 2, rmat_s),
		.correlation = core::DoubleArray2D(2, 2, corr_mat),
		.variances = {0.99286345, 0.007136544}
		});

	tmat_m = { 0.17266529 };
	itmat_w = { 5.7915517 };
	rmat_s = { 7.893867E-17, -0.55275, -0.300238, 2.039184, 0.047723, 0.3002382, -1.534153 };
	corr_mat = { 1.0 };

	levels.emplace(2, HierarchicalLevel{
		.transition = core::DoubleArray2D(1, 1, tmat_m),
		.inverse_transition = core::DoubleArray2D(1, 1, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 1, rmat_s),
		.correlation = core::DoubleArray2D(1, 1, corr_mat),
		.variances = {1.0}
		});

	return DynamicHierarchicalLinearModel(std::move(models), std::move(levels));
}