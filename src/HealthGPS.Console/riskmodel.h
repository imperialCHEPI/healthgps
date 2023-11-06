#pragma once
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace host::poco {
/*
 * POCO data types for models's JSON file parsing
 *
 * 1. Full hierarchical linear models definition
 */

struct CoefficientInfo {
    double value{};
    double pvalue{};
    double tvalue{};
    double std_error{};
};

struct LinearModelInfo {
    std::string formula;
    std::unordered_map<std::string, CoefficientInfo> coefficients;
    double residuals_standard_deviation{};
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

/*
 * 2. Lite hierarchical linear models (Energy Balance) definition
 */

struct VariableInfo {
    std::string name;
    std::string factor;
};

struct FactorDynamicEquationInfo {
    std::string name;
    std::map<std::string, double> coefficients;
    double residuals_standard_deviation{};
};

struct LiteHierarchicalModelInfo {
    double percentage{};
    std::vector<VariableInfo> variables;
    std::map<std::string, std::map<std::string, std::vector<FactorDynamicEquationInfo>>> equations;
};
} // namespace host::poco
