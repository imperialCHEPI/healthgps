#pragma once

#include "linear_model_eval_options.h"
#include "static_linear_model.h"

namespace hgps {
double evaluate_linear_model(const Person &person, const LinearModelParams &model,
                             const LinearModelEvalOptions &options = {});

/// @brief Resolve one predictor for linear models (respects capped_age in options).
double get_linear_predictor_value(const Person &person, const core::Identifier &name,
                                  const LinearModelEvalOptions &options = {});

} // namespace hgps
