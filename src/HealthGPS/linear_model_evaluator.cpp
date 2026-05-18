#include "linear_model_evaluator.h"

#include "predictor_resolver.h"

#include <cmath>

namespace hgps {
namespace {

bool is_capped_age_predictor(const core::Identifier &name) {
    const std::string key = name.to_string();
    return key.starts_with("age") || key.starts_with("Age");
}

int capped_age_power(const std::string &key) {
    if (key == "age" || key == "Age" || key == "age1") {
        return 1;
    }
    if (key == "age2" || key == "Age2") {
        return 2;
    }
    if (key == "age3" || key == "Age3") {
        return 3;
    }
    if (key.size() > 3) {
        try {
            return std::stoi(key.substr(3));
        } catch (...) {
            return 1;
        }
    }
    return 1;
}

double capped_age_term(const std::string &key, double capped_age) {
    return std::pow(capped_age, capped_age_power(key));
}

double predictor_contribution(const Person &person, const core::Identifier &name, double coef,
                              const LinearModelEvalOptions &options) {
    try {
        return coef * get_linear_predictor_value(person, name, options);
    } catch (const std::exception &) {
        if (options.missing_predictor_fallback) {
            if (auto fallback = options.missing_predictor_fallback(name)) {
                return coef * *fallback;
            }
        }
        throw;
    }
}

double log_predictor_contribution(const Person &person, const core::Identifier &name, double coef,
                                  const LinearModelEvalOptions &options) {
    double value = 0.0;
    try {
        value = get_linear_predictor_value(person, name, options);
    } catch (const std::exception &) {
        if (options.missing_predictor_fallback) {
            if (auto fallback = options.missing_predictor_fallback(name)) {
                value = *fallback;
            } else {
                throw;
            }
        } else {
            throw;
        }
    }
    if (value <= 0.0) {
        value = 1e-10;
    }
    return coef * std::log(value);
}

} // namespace

double get_linear_predictor_value(const Person &person, const core::Identifier &name,
                                  const LinearModelEvalOptions &options) {
    if (options.capped_age.has_value() && is_capped_age_predictor(name)) {
        return capped_age_term(name.to_string(), *options.capped_age);
    }

    if (auto derived = resolve_derived_predictor(person, name.to_string())) {
        return *derived;
    }

    return person.get_risk_factor_value(name);
}

double evaluate_linear_model(const Person &person, const LinearModelParams &model,
                             const LinearModelEvalOptions &options) {
    double linear = model.intercept;

    for (const auto &[name, coef] : model.coefficients) {
        if (is_metadata_predictor(name)) {
            continue;
        }
        linear += predictor_contribution(person, name, coef, options);
    }

    for (const auto &[name, coef] : model.log_coefficients) {
        linear += log_predictor_contribution(person, name, coef, options);
    }

    return linear;
}

} // namespace hgps
