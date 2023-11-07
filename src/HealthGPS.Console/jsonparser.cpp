#include "jsonparser.h"

namespace host::poco {
//--------------------------------------------------------
// Risk Model JSON serialisation / de-serialisation
//--------------------------------------------------------
// Data file information
void to_json(json &j, const FileInfo &p) {
    j = json{
        {"name", p.name}, {"format", p.format}, {"delimiter", p.delimiter}, {"columns", p.columns}};
}

// Linear models
void to_json(json &j, const CoefficientInfo &p) {
    j = json{
        {"value", p.value}, {"stdError", p.std_error}, {"tValue", p.tvalue}, {"pValue", p.pvalue}};
}

void from_json(const json &j, CoefficientInfo &p) {
    j.at("value").get_to(p.value);
    j.at("stdError").get_to(p.std_error);
    j.at("tValue").get_to(p.tvalue);
    j.at("pValue").get_to(p.pvalue);
}

void to_json(json &j, const LinearModelInfo &p) {
    j = json{{"formula", p.formula},
             {"coefficients", p.coefficients},
             {"residualsStandardDeviation", p.residuals_standard_deviation},
             {"rSquared", p.rsquared}};
}

void from_json(const json &j, LinearModelInfo &p) {
    j.at("formula").get_to(p.formula);
    j.at("coefficients").get_to(p.coefficients);
    j.at("residualsStandardDeviation").get_to(p.residuals_standard_deviation);
    j.at("rSquared").get_to(p.rsquared);
}

// Hierarchical levels
void to_json(json &j, const Array2Info &p) {
    j = json{{"rows", p.rows}, {"cols", p.cols}, {"data", p.data}};
}

void from_json(const json &j, Array2Info &p) {
    j.at("rows").get_to(p.rows);
    j.at("cols").get_to(p.cols);
    j.at("data").get_to(p.data);
}

void to_json(json &j, const HierarchicalLevelInfo &p) {
    j = json{{"variables", p.variables},     {"m", p.transition},
             {"w", p.inverse_transition},    {"s", p.residual_distribution},
             {"correlation", p.correlation}, {"variances", p.variances}};
}

void from_json(const json &j, HierarchicalLevelInfo &p) {
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

// Settings Information
void to_json(json &j, const SettingsInfo &p) {
    j = json{{"country_code", p.country},
             {"size_fraction", p.size_fraction},
             {"age_range", p.age_range}};
}

void from_json(const json &j, SettingsInfo &p) {
    j.at("country_code").get_to(p.country);
    j.at("size_fraction").get_to(p.size_fraction);
    j.at("age_range").get_to(p.age_range);
}

// Risk factor modelling
void to_json(json &j, const RiskFactorInfo &p) {
    j = json{{"name", p.name}, {"level", p.level}, {"range", p.range}};
}

void from_json(const json &j, RiskFactorInfo &p) {
    j.at("name").get_to(p.name);
    j.at("level").get_to(p.level);
    j.at("range").get_to(p.range);
}

// SES Model Information
void to_json(json &j, const SESInfo &p) {
    j = json{{"function_name", p.function}, {"function_parameters", p.parameters}};
}

void from_json(const json &j, SESInfo &p) {
    j.at("function_name").get_to(p.function);
    j.at("function_parameters").get_to(p.parameters);
}

void to_json(json &j, const VariableInfo &p) { j = json{{"Name", p.name}, {"Factor", p.factor}}; }

void from_json(const json &j, VariableInfo &p) {
    j.at("Name").get_to(p.name);
    j.at("Factor").get_to(p.factor);
}

void to_json(json &j, const FactorDynamicEquationInfo &p) {
    j = json{{"Name", p.name},
             {"Coefficients", p.coefficients},
             {"ResidualsStandardDeviation", p.residuals_standard_deviation}};
}

void from_json(const json &j, FactorDynamicEquationInfo &p) {
    j.at("Name").get_to(p.name);
    j.at("Coefficients").get_to(p.coefficients);
    j.at("ResidualsStandardDeviation").get_to(p.residuals_standard_deviation);
}

// Baseline scenario adjustments
void to_json(json &j, const BaselineInfo &p) {
    j = json{{"format", p.format},
             {"delimiter", p.delimiter},
             {"encoding", p.encoding},
             {"file_names", p.file_names}};
}

// Policy Scenario
void to_json(json &j, const PolicyPeriodInfo &p) {
    j = json{{"start_time", p.start_time}, {"finish_time", p.to_finish_time_str()}};
}

void from_json(const json &j, PolicyPeriodInfo &p) {
    j.at("start_time").get_to(p.start_time);
    if (!j.at("finish_time").is_null() && !j.at("finish_time").empty()) {
        p.finish_time = j.at("finish_time").get<int>();
    }
}

void to_json(json &j, const PolicyImpactInfo &p) {
    j = json{{"risk_factor", p.risk_factor},
             {"impact_value", p.impact_value},
             {"from_age", p.from_age},
             {"to_age", p.to_age_str()}};
}

void from_json(const json &j, PolicyImpactInfo &p) {
    j.at("risk_factor").get_to(p.risk_factor);
    j.at("impact_value").get_to(p.impact_value);
    j.at("from_age").get_to(p.from_age);
    if (!j.at("to_age").is_null() && !j.at("to_age").empty()) {
        p.to_age = j.at("to_age").get<int>();
    }
}

void to_json(json &j, const PolicyAdjustmentInfo &p) {
    j = json{{"risk_factor", p.risk_factor}, {"value", p.value}};
}

void from_json(const json &j, PolicyAdjustmentInfo &p) {
    j.at("risk_factor").get_to(p.risk_factor);
    j.at("value").get_to(p.value);
}

void to_json(json &j, const PolicyScenarioInfo &p) {
    j = json{{"active_period", p.active_period},
             {"impact_type", p.impact_type},
             {"dynamics", p.dynamics},
             {"coefficients", p.coefficients},
             {"coverage_rates", p.coverage_rates},
             {"coverage_cutoff_time", p.to_coverage_cutoff_time_str()},
             {"child_cutoff_age", p.to_child_cutoff_age_str()},
             {"adjustments", p.adjustments},
             {"impacts", p.impacts}};
}

void from_json(const json &j, PolicyScenarioInfo &p) {
    j.at("active_period").get_to(p.active_period);
    if (j.contains("impact_type")) {
        j.at("impact_type").get_to(p.impact_type);
    }

    if (j.contains("dynamics")) {
        j.at("dynamics").get_to(p.dynamics);
    }

    if (j.contains("coefficients")) {
        j.at("coefficients").get_to(p.coefficients);
    }

    if (j.contains("coverage_rates")) {
        j.at("coverage_rates").get_to(p.coverage_rates);
    }

    if (j.contains("coverage_cutoff_time")) {
        p.coverage_cutoff_time = j.at("coverage_cutoff_time").get<unsigned int>();
    }

    if (j.contains("child_cutoff_age")) {
        p.child_cutoff_age = j.at("child_cutoff_age").get<unsigned int>();
    }

    if (j.contains("adjustments")) {
        j.at("adjustments").get_to(p.adjustments);
    }

    j.at("impacts").get_to(p.impacts);
}

// Result information
void to_json(json &j, const OutputInfo &p) {
    j = json{{"folder", p.folder}, {"file_name", p.file_name}, {"comorbidities", p.comorbidities}};
}

void from_json(const json &j, OutputInfo &p) {
    j.at("folder").get_to(p.folder);
    j.at("file_name").get_to(p.file_name);
    j.at("comorbidities").get_to(p.comorbidities);
}
} // namespace host::poco
