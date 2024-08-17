#pragma once
#include <fstream>

#include "HealthGPS.Input/jsonparser.h"
#include "HealthGPS/riskfactor.h"

template <typename TYPE>
std::string join_string(const std::vector<TYPE> &v, std::string_view delimiter,
                        bool quoted = false);

std::string join_string_map(const std::vector<std::string> &v, std::string_view delimiter);
