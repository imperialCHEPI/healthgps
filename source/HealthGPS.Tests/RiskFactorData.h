#pragma once
#include <format>
#include <fstream>

#include "HealthGPS/riskfactor.h"
#include "HealthGPS.Console/jsonparser.h"

std::string join_string(const std::vector<double>& v, std::string_view delimiter) {
	std::stringstream  s;
	if (!v.empty()) {
		s << v.front();
		for (size_t i = 1; i < v.size(); ++i) {
			s << delimiter << " " << v[i];
		}
	}

	return s.str();
}

std::string generate_test_code(hgps::HierarchicalModelType model_type, std::string filename) {
	std::stringstream ss;
	HierarchicalModelInfo hmodel;
	std::ifstream ifs(filename, std::ifstream::in);
	if (ifs) {
		try {
			auto opt = json::parse(ifs);
			hmodel.models = opt["models"].get<std::unordered_map<std::string, LinearModelInfo>>();
			hmodel.levels = opt["levels"].get<std::unordered_map<std::string, HierarchicalLevelInfo>>();

			ss << "using namespace hgps;\n";

			ss << "\n/*---- LINEAR MODELS ---- */\n";
			ss << "std::unordered_map<std::string, LinearModel> models;\n";
			ss << "std::unordered_map<std::string, Coefficient> coeffs;\n";
			for (auto& item : hmodel.models) {
				auto& at = item.second;
				ss << std::format("\n/* {} */\n", item.first);
				for (auto& entry : at.coefficients) {
					ss << std::format("coeffs.emplace(\"{}\", Coefficient{{ .value={}, .pvalue={}, .tvalue={}, .std_error={} }});\n",
						entry.first, std::to_string(entry.second.value), std::to_string(entry.second.pvalue),
						std::to_string(entry.second.tvalue), std::to_string(entry.second.std_error));
				}

				ss << std::format("models.emplace(\"{}\", LinearModel{{ \n\t.coefficients=coeffs, \
							      \n\t.fitted_values={{{}}},\n\t.residuals={{{}}},\n\t.rsquared={} }});\n",
					item.first, join_string(at.fitted_values, ","), join_string(at.residuals, ","), std::to_string(at.rsquared));

				ss << "coeffs.clear();\n";
			}

			ss << "\n/*---- HIERARCHICAL LEVELS  ---- */\n";
			ss << "std::map<int, HierarchicalLevel> levels;\n";
			ss << "std::vector<double> tmat_m;\n";
			ss << "std::vector<double> itmat_w;\n";
			ss << "std::vector<double> rmat_s;\n";
			ss << "std::vector<double> corr_mat;\n";
			for (auto& item : hmodel.levels) {
				auto& at = item.second;
				ss << std::format("\n/* {} */\n", item.first);
				ss << std::format("tmat_m = {{{}}};\n", join_string(at.transition.data, ","));
				ss << std::format("itmat_w = {{{}}};\n", join_string(at.inverse_transition.data, ","));
				ss << std::format("rmat_s = {{{}}};\n", join_string(at.residual_distribution.data, ","));
				ss << std::format("corr_mat = {{{}}};\n", join_string(at.correlation.data, ","));

				ss << std::format("levels.emplace({0}, HierarchicalLevel{{ \
								  \n\t.transition = core::DoubleArray2D({1}, {1}, tmat_m), \
					              \n\t.inverse_transition = core::DoubleArray2D({1}, {1}, itmat_w), \
								  \n\t.residual_distribution = core::DoubleArray2D({2}, {1}, rmat_s), \
								  \n\t.correlation = core::DoubleArray2D({1}, {1}, corr_mat),\n\t.variances = {{{3}}} }});\n",
					item.first, at.transition.rows, at.residual_distribution.rows, join_string(at.variances, ","));
			}

			if (model_type == hgps::HierarchicalModelType::Static) {
				ss << "\nreturn HierarchicalLinearModel(std::move(models), std::move(levels));\n";
			}
			else {
				ss << "\nreturn DynamicHierarchicalLinearModel(std::move(models), std::move(levels));\n";
			}
		}
		catch (const std::exception& ex) {
			std::cout << std::format("Failed to parse model file: {}. {}\n", filename, ex.what());
		}
	}
	else {
		std::cout << std::format("Model file: {} not found.\n", filename);
	}

	ifs.close();
	return ss.str();
}

hgps::HierarchicalLinearModel get_static_test_model() {
	/* Auto-generated code, do not change **** */

	using namespace hgps;

	/*---- LINEAR MODELS ---- */
	std::unordered_map<std::string, LinearModel> models;
	std::unordered_map<std::string, Coefficient> coeffs;

	/* AlcoholConsumption */
	coeffs.emplace("Age", Coefficient{ .value = -0.118861, .pvalue = 0.377382, .tvalue = -0.991924, .std_error = 0.119829 });
	coeffs.emplace("Gender", Coefficient{ .value = 16.466127, .pvalue = 0.010112, .tvalue = 4.589376, .std_error = 3.587879 });
	coeffs.emplace("Intercept", Coefficient{ .value = 6.382514, .pvalue = 0.405954, .tvalue = 0.927959, .std_error = 6.878011 });
	models.emplace("AlcoholConsumption", LinearModel{
		.coefficients = coeffs,
		.fitted_values = {-0.0360014, 14.5283, -2.53209, 13.8152, 16.6678, 2.57895, 14.6472},
		.residuals = {0.0398585, 1.32737, 2.53209, -2.39231, 5.54929, -2.57195, -4.48434},
		.rsquared = 0.848093 });
	coeffs.clear();

	/* SmokingStatus */
	coeffs.emplace("Age", Coefficient{ .value = -0.000268, .pvalue = 0.982634, .tvalue = -0.023157, .std_error = 0.011577 });
	coeffs.emplace("Gender", Coefficient{ .value = -0.663159, .pvalue = 0.128275, .tvalue = -1.913180, .std_error = 0.346627 });
	coeffs.emplace("Intercept", Coefficient{ .value = 0.681054, .pvalue = 0.363331, .tvalue = 1.024930, .std_error = 0.664488 });
	models.emplace("SmokingStatus", LinearModel{
		.coefficients = coeffs,
		.fitted_values = {0.666577, -0.000871255, 0.660948, -0.00247973, 0.00395416, 0.672475, -0.000603177},
		.residuals = {-0.666577, 0.000871255, 0.339052, 0.00247973, -0.00395416, 0.327525, 0.000603177},
		.rsquared = 0.533396 });
	coeffs.clear();

	/* BMI */
	coeffs.emplace("AlcoholConsumption", Coefficient{ .value = 0.319474, .pvalue = 0.131175, .tvalue = 2.481636, .std_error = 0.128735 });
	coeffs.emplace("Age", Coefficient{ .value = 0.039381, .pvalue = 0.329978, .tvalue = 1.276441, .std_error = 0.030852 });
	coeffs.emplace("Gender", Coefficient{ .value = -0.057628, .pvalue = 0.955930, .tvalue = -0.062385, .std_error = 0.923759 });
	coeffs.emplace("Intercept", Coefficient{ .value = 22.407962, .pvalue = 0.006188, .tvalue = 12.653734, .std_error = 1.770858 });
	coeffs.emplace("SmokingStatus", Coefficient{ .value = -10.048320, .pvalue = 0.017135, .tvalue = -7.540840, .std_error = 1.332520 });
	models.emplace("BMI", LinearModel{
		.coefficients = coeffs,
		.fitted_values = {31.2452, 25.5223, 22.7636, 24.5541, 26.2107, 19.5554, 23.6289},
		.residuals = {-5.55112e-17, -1.30084, 0.176199, 0.475097, 0.564788, -0.176199, 0.260957},
		.rsquared = 0.970226 });
	coeffs.clear();

	/*---- HIERARCHICAL LEVELS  ---- */
	std::map<int, HierarchicalLevel> levels;
	std::vector<double> tmat_m;
	std::vector<double> itmat_w;
	std::vector<double> rmat_s;
	std::vector<double> corr_mat;

	/* 1 */
	tmat_m = { -0.0035606, 3.19407, 0.308565, 0.0184036 };
	itmat_w = { -0.0186717, 3.24059, 0.313059, 0.00361245 };
	rmat_s = { 0.0249242, 0.415528, 0.786364, -0.748982, 1.73733, -0.811288, -1.40388, -2.15996, 0.00761843, 1.10788, -0.000606345, 0.00723274, 1.05208, -0.0142448 };
	corr_mat = { 1, -0.0057769, -0.0057769, 1 };
	levels.emplace(1, HierarchicalLevel{
		.transition = core::DoubleArray2D(2, 2, tmat_m),
		.inverse_transition = core::DoubleArray2D(2, 2, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 2, rmat_s),
		.correlation = core::DoubleArray2D(2, 2, corr_mat),
		.variances = {0.990721, 0.00927894} });

	/* 2 */
	tmat_m = { 0.581511 };
	itmat_w = { 1.71966 };
	rmat_s = { -1.15916e-16, -2.237, 0.303002, 0.817005, 0.971242, -0.303002, 0.448757 };
	corr_mat = { 1 };
	levels.emplace(2, HierarchicalLevel{
		.transition = core::DoubleArray2D(1, 1, tmat_m),
		.inverse_transition = core::DoubleArray2D(1, 1, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 1, rmat_s),
		.correlation = core::DoubleArray2D(1, 1, corr_mat),
		.variances = {1} });

	return HierarchicalLinearModel(std::move(models), std::move(levels));
}

hgps::DynamicHierarchicalLinearModel get_dynamic_test_model() {
	/* Auto-generated code, do not change **** */

	using namespace hgps;

	/*---- LINEAR MODELS ---- */
	std::unordered_map<std::string, LinearModel> models;
	std::unordered_map<std::string, Coefficient> coeffs;

	/* AlcoholConsumption */
	coeffs.emplace("Year", Coefficient{ .value = 1.493626, .pvalue = 0.458706, .tvalue = 0.848063, .std_error = 1.761221 });
	coeffs.emplace("Age", Coefficient{ .value = -0.148091, .pvalue = 0.334091, .tvalue = -1.148338, .std_error = 0.128961 });
	coeffs.emplace("Gender", Coefficient{ .value = 15.852800, .pvalue = 0.024910, .tvalue = 4.182263, .std_error = 3.790484 });
	coeffs.emplace("Intercept", Coefficient{ .value = -2990.254569, .pvalue = 0.459571, .tvalue = -0.846255, .std_error = 3533.514256 });
	models.emplace("AlcoholConsumption", LinearModel{
		.coefficients = coeffs,
		.fitted_values = {-2.03725, 12.9397, -0.666284, 13.5448, 18.5926, 2.71439, 14.5814},
		.residuals = {2.0411, 2.91599, 0.666284, -2.12194, 3.62453, -2.70739, -4.41858},
		.rsquared = 0.877468 });
	coeffs.clear();

	/* SmokingStatus */
	coeffs.emplace("Year", Coefficient{ .value = 0.216439, .pvalue = 0.225850, .tvalue = 1.519946, .std_error = 0.142399 });
	coeffs.emplace("Age", Coefficient{ .value = -0.004504, .pvalue = 0.694950, .tvalue = -0.431936, .std_error = 0.010427 });
	coeffs.emplace("Gender", Coefficient{ .value = -0.752035, .pvalue = 0.091365, .tvalue = -2.453866, .std_error = 0.306470 });
	coeffs.emplace("Intercept", Coefficient{ .value = -433.556037, .pvalue = 0.226411, .tvalue = -1.517559, .std_error = 285.693038 });
	models.emplace("SmokingStatus", LinearModel{
		.coefficients = coeffs,
		.fitted_values = {0.376581, -0.231076, 0.931318, -0.0416594, 0.282869, 0.692101, -0.0101334},
		.residuals = {-0.376581, 0.231076, 0.0686818, 0.0416594, -0.282869, 0.307899, 0.0101334},
		.rsquared = 0.736394 });
	coeffs.clear();

	/* BMI */
	coeffs.emplace("Year", Coefficient{ .value = -1.339372, .pvalue = 0.086731, .tvalue = -7.294724, .std_error = 0.183608 });
	coeffs.emplace("AlcoholConsumption", Coefficient{ .value = 0.192391, .pvalue = 0.213226, .tvalue = 2.873171, .std_error = 0.066961 });
	coeffs.emplace("Age", Coefficient{ .value = 0.065592, .pvalue = 0.128705, .tvalue = 4.878789, .std_error = 0.013444 });
	coeffs.emplace("Gender", Coefficient{ .value = 0.492357, .pvalue = 0.430557, .tvalue = 1.245968, .std_error = 0.395160 });
	coeffs.emplace("Intercept", Coefficient{ .value = 2709.566770, .pvalue = 0.086022, .tvalue = 7.355539, .std_error = 368.370949 });
	coeffs.emplace("SmokingStatus", Coefficient{ .value = -12.011927, .pvalue = 0.043824, .tvalue = -14.503793, .std_error = 0.828192 });
	models.emplace("BMI", LinearModel{
		.coefficients = coeffs,
		.fitted_values = {31.2452, 24.3169, 22.9916, 24.6771, 26.7673, 19.3274, 24.1548},
		.residuals = {1.38778e-17, -0.0954413, -0.0518407, 0.352096, 0.00824012, 0.0518407, -0.264895},
		.rsquared = 0.997375 });
	coeffs.clear();

	/*---- HIERARCHICAL LEVELS  ---- */
	std::map<int, HierarchicalLevel> levels;
	std::vector<double> tmat_m;
	std::vector<double> itmat_w;
	std::vector<double> rmat_s;
	std::vector<double> corr_mat;

	/* 1 */
	tmat_m = { -0.111198, 2.86563, -0.203549, -0.132974 };
	itmat_w = { -0.222334, -4.79136, 0.340336, -0.185925 };
	rmat_s = { 0.778387, 0.94104, 0.21149, -0.731434, 1.29645, -0.989878, -1.50605, 1.42484, -1.64932, -0.452958, 0.194915, 0.681437, -0.971885, 0.772969 };
	corr_mat = { 1, -0.438228, -0.438228, 1 };
	levels.emplace(1, HierarchicalLevel{
		.transition = core::DoubleArray2D(2, 2, tmat_m),
		.inverse_transition = core::DoubleArray2D(2, 2, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 2, rmat_s),
		.correlation = core::DoubleArray2D(2, 2, corr_mat),
		.variances = {0.992863, 0.00713654} });

	/* 2 */
	tmat_m = { 0.172665 };
	itmat_w = { 5.79155 };
	rmat_s = { 7.89387e-17, -0.552753, -0.300238, 2.03918, 0.0477231, 0.300238, -1.53415 };
	corr_mat = { 1 };
	levels.emplace(2, HierarchicalLevel{
		.transition = core::DoubleArray2D(1, 1, tmat_m),
		.inverse_transition = core::DoubleArray2D(1, 1, itmat_w),
		.residual_distribution = core::DoubleArray2D(7, 1, rmat_s),
		.correlation = core::DoubleArray2D(1, 1, corr_mat),
		.variances = {1} });

	return DynamicHierarchicalLinearModel(std::move(models), std::move(levels));
}