#include "linear_model_evaluator.h"

#include "predictor_resolver.h"

#include <cmath>

namespace hgps {
namespace {

bool is_capped_age_predictor(const core::Identifier &name) {
    const auto key = name.to_string();
    return key.starts_with("age") || key.starts_with("Age");
}

double capped_age_term(const std::string &key, double capped_age) {
    int power = 1;
    if (key == "age" || key == "Age" || key == "age1") {
        power = 1;
    } else if (key == "age2" || key == "Age2") {
        power = 2;
    } else if (key == "age3" || key == "Age3") {
        power = 3;
    } else if (key.starts_with("age") && key.size() > 3) {
        try {
            power = std::stoi(key.substr(3));
        } catch (...) {
            power = 1;
        }
    } else if (key.starts_with("Age") && key.size() > 3) {
        try {
            power = std::stoi(key.substr(3));
        } catch (...) {
            power = 1;
        }
    } else {
        return capped_age;
    }
    return std::pow(capped_age, power);
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
        try {
            linear += coef * get_linear_predictor_value(person, name, options);
        } catch (const std::exception &) {
            if (options.missing_predictor_fallback) {
                if (auto fallback = options.missing_predictor_fallback(name)) {
                    linear += coef * *fallback;
                    continue;
                }
            }
            throw;
        }
    }

    for (const auto &[name, coef] : model.log_coefficients) {
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
        linear += coef * std::log(value);
    }

    return linear;
}

} // namespace hgps
